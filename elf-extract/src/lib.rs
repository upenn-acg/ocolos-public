use std::os::raw::c_char;
use object::{Endianness, File, Object, ObjectSymbol, Symbol};
use std::collections::{HashMap, HashSet};
use std::fs;
use std::ffi::CStr;
use std::io::{BufWriter, Write};
use object::elf::FileHeader64;
use object::read::elf::FileHeader;
use once_cell::sync::OnceCell;

const DEBUG: bool = false;

static FILE_BYTES: OnceCell<Vec<u8>> = OnceCell::new();
static ELF_FILE: OnceCell<File> = OnceCell::new();
static ELF_SECTIONS: OnceCell<Vec<(u64,u64)>> = OnceCell::new();
static ELF_SYMBOLS: OnceCell<HashMap<u64,Symbol>> = OnceCell::new();

/** Parse the BOLTed binary at `bolted_binary_path`, extracting vtables and writing them in binary
    format to the file at `output_path`. */
#[no_mangle]
pub extern "C" fn write_vtable(bolted_binary_path: *const c_char, output_file_path: *const c_char) {
    let bolted_binary_path = unsafe { CStr::from_ptr(bolted_binary_path).to_str().unwrap().to_owned() };
    let vtabs = get_vtables(&bolted_binary_path).expect("problem parsing BOLTed binary");

    let output_file_path = unsafe { CStr::from_ptr(output_file_path).to_str().unwrap().to_owned() };
    let mut w = BufWriter::new(fs::File::create(&output_file_path).unwrap());
    for vt in vtabs {
        w.write_all(&vt.addr.to_le_bytes()).unwrap();
        w.write_all(&(vt.contents.len() as u64).to_le_bytes()).unwrap();
        w.write_all(vt.contents.as_slice()).unwrap();
    }
}

/** Parse the BOLTed binary at `bolted_binary_path`, then extract the machine code for each function
   listed in `addrs` and write the machine code in binary format to the file at `output_path`.
   `addrs` is a C array of function starting addresses, containing `num_addrs` elements. */
#[no_mangle]
pub extern "C" fn write_functions(bolted_binary_path: *const c_char,
                                  output_file_path: *const c_char,
                                  addrs: *const u64,
                                  num_addrs: u64) {
    let bolted_binary_path = unsafe { CStr::from_ptr(bolted_binary_path).to_str().unwrap().to_owned() };
    init(&bolted_binary_path).unwrap();
    let addrs = unsafe { std::slice::from_raw_parts(addrs, num_addrs as usize) };

    let output_file_path = unsafe { CStr::from_ptr(output_file_path).to_str().unwrap().to_owned() };
    let mut w = BufWriter::new(fs::File::create(&output_file_path).unwrap());

    for a in addrs {
        if !ELF_SYMBOLS.get().unwrap().contains_key(a) {
            println!("ERROR: couldn't find symbol for function address {:#x}", a);
            continue;
        }
        let sym = ELF_SYMBOLS.get().unwrap().get(a).unwrap();
        let section_idx = sym.section().index().expect(&format!("{:#x} {}",a,sym.name().unwrap())).0;
        let (base_section_addr,base_file_offset) = ELF_SECTIONS.get().unwrap()[section_idx];
        let sym_offset = sym.address().checked_sub(base_section_addr).expect(&format!("underflow with sym {:?}", sym));
        let file_offset_start = (base_file_offset + sym_offset) as usize;
        let bytes: &[u8] = &FILE_BYTES.get().unwrap()[file_offset_start..file_offset_start+sym.size() as usize];

        w.write_all(&a.to_le_bytes()).unwrap();
        w.write_all(&(bytes.len() as u64).to_le_bytes()).unwrap();
        w.write_all(bytes).unwrap();
    }
}

pub struct Vtable {
    /** where the vtable begins in the address space */
    pub addr: u64,
    /** the binary contents of the vtable */
    pub contents: Vec<u8>,
}

/** Parse the given BOLTed binary and initialize some global data structures based on its contents.
    This avoids redundant overhead from repeatedly parsing the binary on each write_*() call. */
fn init(bolted_bin_path: &str) -> Result<(),()> {
    if ELF_SECTIONS.get().is_some() {
        // initialization already done!
        assert!(FILE_BYTES.get().is_some());
        assert!(ELF_FILE.get().is_some());
        assert!(ELF_SYMBOLS.get().is_some());
        return Ok(())
    }

    let file_bytes = fs::read(bolted_bin_path).unwrap();
    let mut sections: Vec<(u64,u64)> = vec![]; // elements are (section_address, section_file_offset)
    {
        if let Ok(elf) = FileHeader64::<Endianness>::parse(&*file_bytes) {

            for sh in elf.section_headers(elf.endian().unwrap(), &*file_bytes).unwrap() {
                // where the section should begin in the address space
                let sec_addr = sh.sh_addr.get(elf.endian().unwrap());
                // the file offset to the start of the section in the binary file
                let sec_offset = sh.sh_offset.get(elf.endian().unwrap());
                sections.push( (sec_addr, sec_offset) );
                if DEBUG {
                    println!("address {:#x} has file offset {:#x}", sec_addr, sec_offset);
                }
            }
        }
    }
    ELF_SECTIONS.set(sections).unwrap();
    FILE_BYTES.set(file_bytes).unwrap();

    let file = match object::File::parse(FILE_BYTES.get().unwrap().as_slice()) {
        Ok(file) => file,
        Err(err) => {
            println!("Failed to parse file '{}': {}", bolted_bin_path, err);
            return Err(());
        }
    };

    ELF_FILE.set(file).unwrap();

    let mut m: HashMap<u64,Symbol> = HashMap::new();
    ELF_FILE.get().unwrap().symbols().chain(ELF_FILE.get().unwrap().dynamic_symbols())
        .for_each( |s| {
            if object::SymbolKind::Text == s.kind() {
                m.insert(s.address(), s);
            }
        });
    ELF_SYMBOLS.set(m).unwrap();

    return Ok(())
}

pub fn get_vtables(bolted_bin_path: &str) -> Result<Vec<Vtable>,()> {
    init(bolted_bin_path)?;

    // let section_kinds = file.sections().map(|s| (s.index(), s.kind())).collect();
    // for symbol in file.symbols() {
    //     print_symbol(&symbol, &section_kinds);
    // }
    // for symbol in file.dynamic_symbols() {
    //     print_symbol(&symbol, &section_kinds);
    // }

    let mut vtabs: Vec<Vtable> = vec![];
    let mut vtab_addrs: HashSet<u64> = HashSet::new();

    for symbol in ELF_FILE.get().unwrap().symbols().chain(ELF_FILE.get().unwrap().dynamic_symbols()) {
        if let Some(vt) = extract_vtable(&symbol,
                                         ELF_SECTIONS.get().unwrap().as_slice(),
                                         FILE_BYTES.get().unwrap().as_slice()) {
            if DEBUG {
                // verify that vtable contents are identical across duplicates
                if vtab_addrs.contains(&vt.addr) {
                    let found = vtabs.iter().any(|e| {
                        return e.addr == vt.addr && e.contents == vt.contents;
                    });
                    assert!(found);
                }
            }
            if !vtab_addrs.contains(&vt.addr) {
                vtab_addrs.insert(vt.addr);
                vtabs.push(vt);
            }
        }
    }

    return Ok(vtabs);
}

fn extract_vtable(symbol: &Symbol<'_, '_>,
                  sections: &[(u64,u64)],
                  file_bytes: &[u8]) -> Option<Vtable> {
    // only extract v-tables
    if let Ok(sym_name) = symbol.name() {
        if !sym_name.starts_with("_ZTV") && !sym_name.starts_with("_ZTC") {
            return None;
        }
    } else {
        return None;
    }

    if symbol.is_undefined() {
        if DEBUG {
            println!("undefined vtable {}", symbol.name().unwrap());
        }
        return None;
    }
    let section_idx = symbol.section().index().expect(symbol.name().unwrap()).0;
    let (base_section_addr,base_file_offset) = sections[section_idx];
    let file_offset_start = (base_file_offset + (symbol.address() - base_section_addr)) as usize;
    let bytes: Vec<u8> = file_bytes[file_offset_start..file_offset_start+symbol.size() as usize].to_vec();

    return Some(Vtable { addr: symbol.address(), contents: bytes });
}

