#!/bin/bash

OCOLOS_PATH=/home/ocolos/ocolos-public
OCOLOS_DATA_PATH=/home/ocolos/ocolos_data
BOLT_PATH=/home/ocolos/BOLT/build
MYSQL_BASE_PATH=/usr/local/mysql
RUN_BENCHMARK_CMD="/usr/bin/sysbench /usr/share/sysbench/oltp_read_only.lua --threads=4 --events=100000000 --time=60 --mysql-host=127.0.0.1 --mysql-port=3306 --mysql-user=ocolos --mysql-db=ocolos_db --tables=4 --table-size=10000 --range_selects=off --db-ps-mode=disable --report-interval=1 run"

###############################

mysqladmin -u root shutdown
#produce the first mysqld.bolt binary
${OCOLOS_PATH}/tracer
# produce the second mysqld.bolt binary
mysqladmin -u root shutdown
${BOLT_PATH}/bin/perf2bolt --ignore-build-id --cont-opt --call-stack-func=${OCOLOS_DATA_PATH}/callstack_func.bin --bin-path-info=${OCOLOS_DATA_PATH}/BOLTed_bin_info.txt -p ${OCOLOS_DATA_PATH}/perf2.data -o ${OCOLOS_DATA_PATH}/perf2.fdata ${OCOLOS_DATA_PATH}/mysqld.bolt
${BOLT_PATH}/bin/llvm-bolt ${MYSQL_BASE_PATH}/bin/mysqld -o ${OCOLOS_DATA_PATH}/mysqld2.bolt --enable-bat --enable-func-map-table -data=${OCOLOS_DATA_PATH}/perf2.fdata -reorder-blocks=cache+ -reorder-functions=hfsort 
mv ${OCOLOS_DATA_PATH}/mysqld2.bolt ${MYSQL_BASE_PATH}/bin
${MYSQL_BASE_PATH}/bin/mysqld2.bolt --user=ocolos --port=3306 --basedir=${MYSQL_BASE_PATH} --datadir=${MYSQL_BASE_PATH}/data --plugin-dir=${MYSQL_BASE_PATH}/lib/plugin --log-error=ocolos.err --pid-file=ocolos.pid &
PID=$!
sleep 15
$RUN_BENCHMARK_CMD
