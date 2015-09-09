echo 'main="$1"; shift'
echo exec "$LD" ' -lpthread -o "$main" "$main".o ${1+"$@"}'
