use object::{ObjectSymbol, SectionIndex, SectionKind, Symbol, SymbolKind, SymbolSection};
use std::collections::HashMap;
use std::{env, fs, process};
use elf_extract::{get_vtables, Vtable};

//const DEBUG: bool = false;

fn main() {
    let arg_len = env::args().len();
    if arg_len <= 1 {
        eprintln!("Usage: {} <file> ...", env::args().next().unwrap());
        process::exit(1);
    }

    for file_path in env::args().skip(1) {
        if arg_len > 2 {
            println!();
            println!("{}:", file_path);
        }

        let jd = get_vtables(&file_path).unwrap();
        let yz: Vec<(u64,Vec<u8>)> = parse_yz();

        // compare jd and vz vtable lists
        for (yz_addr,yz_bytes) in yz {
            let hit: Vec<&Vtable> = jd.iter().filter(|jdvt| {
                let jd_range = jdvt.addr..jdvt.addr+jdvt.contents.len() as u64;
                return jd_range.contains(&yz_addr) &&
                    jd_range.contains(&(yz_addr + yz_bytes.len() as u64 - 1));
            }).collect();
            if hit.is_empty() {
                println!("ERROR: no match for {:#x} bytes starting at {:#x} ", yz_bytes.len(), yz_addr);
                continue;
            }

            //println!("   matched {} bytes starting at {:#x} ", yz_bytes.len(), yz_addr);
            // compare bytes
            let hits_str: Vec<String> = hit.iter().map(|e| {
                return format!("{:#x}_{}elements", e.addr, e.contents.len());
            }).collect();
            assert_eq!(hit.len(), 1, "yz range matched multiple jd ranges {}", hits_str.join(", "));
            let jd_addr = hit[0].addr;
            let jd_bytes = hit[0].contents.as_slice();
            assert!(yz_addr >= jd_addr, "bad yz addr?");
            let yz_offset = (yz_addr - jd_addr) as usize;
            assert_eq!(yz_bytes[..], jd_bytes[yz_offset..yz_offset+yz_bytes.len()], "bytes mismatch")
        }
    }
}

#[allow(dead_code)]
fn print_symbol(symbol: &Symbol<'_, '_>, section_kinds: &HashMap<SectionIndex, SectionKind>) {
    if let SymbolKind::Section | SymbolKind::File = symbol.kind() {
        return;
    }

    // only extract v-tables
    if let Ok(sym_name) = symbol.name() {
        if !sym_name.starts_with("_ZTV") {
            return;
        }
    } else {
        return;
    }

    let mut kind = match symbol.section() {
        SymbolSection::Undefined => 'U',
        SymbolSection::Absolute => 'A',
        SymbolSection::Common => 'C',
        SymbolSection::Section(index) => match section_kinds.get(&index) {
            Some(SectionKind::Text) => 't',
            Some(SectionKind::Data) | Some(SectionKind::Tls) | Some(SectionKind::TlsVariables) => {
                'd'
            }
            Some(SectionKind::ReadOnlyData) | Some(SectionKind::ReadOnlyString) => 'r',
            Some(SectionKind::UninitializedData) | Some(SectionKind::UninitializedTls) => 'b',
            Some(SectionKind::Common) => 'C',
            _ => '?',
        },
        _ => '?',
    };

    if symbol.is_global() {
        kind = kind.to_ascii_uppercase();
    }

    if symbol.is_undefined() {
        print!("{:16} ", "");
    } else {
        print!("{:016x} ", symbol.address());
    }
    println!(
        "{:#08x} {} {}",
        symbol.size(),
        kind,
        symbol.name().unwrap_or("<unknown>"),
    );
}

fn parse_yz() -> Vec<(u64,Vec<u8>)> {
    let mut v = vec![];
    let whole_file = fs::read("/Users/devietti/Research/ocolos/elf-extract/v_table.bin").unwrap();

    let mut i = 0;
    while i < whole_file.len() {
        let addr: u64 = unsafe { *(&whole_file[i] as *const u8 as *const u64) };
        let size: usize = unsafe { *(&whole_file[i+8] as *const u8 as *const u64) } as usize;
        let bytes = &whole_file[i+16..i+16+size];
        v.push( (addr,bytes.to_vec()) );
        i += 16+size;
    }

    return v
}
