output="symbols"
if [ -n "$2" ]; then
    output="$2"
fi
echo $1
nm -an --demangle "$1" | grep " [T,t] " | sed -E "s/(.+) [T,t] (.+)/0x\1 \2/" > $output