IR_SRC="$1"
IR_DST="$2"
PASS="$3"
echo "Started: $PASS"
for file in "$IR_SRC"/*.ll; do
    echo "Processing $file"
    SRC_NAME="$(basename "$file")"

    if ! opt-14 -enable-new-pm=0 -load="$PASS" -instr -S -o "$IR_DST/$SRC_NAME" "$file";
    then
        echo "Failed test $SRC_NAME"
    fi
done