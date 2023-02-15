OCOLOS_PATH=/home/zyuxuan/ocolos-public
OCOLOS_DATA_PATH=/home/zyuxuan/ocolos_data
BOLT_PATH=/home/zyuxuan/BOLT-cont-opt/build
MYSQL_BASE_PATH=/usr/local/mysql
run_benchmark_cmd="/usr/bin/sysbench /usr/share/sysbench/oltp_read_only.lua --threads=4 --events=100000000 --time=60 --mysql-host=127.0.0.1 --mysql-port=3306 --mysql-user=ocolos --mysql-db=ocolos_db --tables=4 --table-size=10000 --range_selects=off --db-ps-mode=disable --report-interval=1 run"

###############################

mysqladmin -u root shutdown
#produce the first mysqld.bolt binary
$OCOLOS_PATH/tracer
# produce the second mysqld.bolt binary
mysqladmin -u root shutdown
cmd="$BOLT_PATH/bin/perf2bolt --ignore-build-id --cont-opt --call-stack-func=$OCOLOS_DATA_PATH/callstack_func.bin --bin-path-info=$OCOLOS_DATA_PATH/BOLTed_bin_info.txt -p $OCOLOS_DATA_PATH/perf2.data -o perf2.fdata $OCOLOS_DATA_PATH/mysqld.bolt"
$cmd 
$BOLT_PATH/bin/llvm-bolt $MYSQL_BASE_PATH/bin/mysqld -o mysqld.bolt --enable-bat --enable-func-map-table -data=perf2.fdata -reorder-blocks=cache+ -reorder-functions=hfsort 
mv mysqld.bolt $MYSQL_BASE_PATH/bin
$MYSQL_BASE_PATH/bin/mysqld.bolt --user=ocolos --port=3306 --basedir=$MYSQL_BASE_PATH --datadir=$MYSQL_BASE_PATH/data --plugin-dir=$MYSQL_BASE_PATH/lib/plugin --log-error=ocolos.err --pid-file=ocolos.pid &
PID=$!
sleep 15
$run_benchmark_cmd
