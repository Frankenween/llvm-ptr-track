C_SRC="./c-src"
IR_PATH="./ir"
RESULT="./ir-final"

mkdir "$IR_PATH" "$RESULT"
PASS="$1"
for file in "$C_SRC"/*.c; do
    FILE_NAME="$(basename "$file" .c)"
    clang-14 -S -emit-llvm -o "$IR_PATH/$FILE_NAME.ll" "$file"
done
for file in "$IR_PATH"/*.ll; do
    FILE_NAME="$(basename "$file")"
    opt-14 -enable-new-pm=0 -load="$PASS" -instr -S -o "$RESULT/$FILE_NAME" "$IR_PATH/$FILE_NAME"
    echo
done

