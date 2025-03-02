C_SRC="./c-src"
IR_PATH="./ir"
RESULT="./ir-final"

mkdir "$IR_PATH" "$RESULT"
PASS_PATH="$1"
for file in "$C_SRC"/*.c; do
    FILE_NAME="$(basename "$file" .c)"
    clang-14 -S -emit-llvm -o "$IR_PATH/$FILE_NAME.ll" "$file"
done
for file in "$IR_PATH"/*.ll; do
    FILE_NAME="$(basename "$file")"
    opt-14 -enable-new-pm=0 -load="$PASS_PATH/purge_stores.so" -remove-store -S -o "$RESULT/$FILE_NAME" "$IR_PATH/$FILE_NAME"
    opt-14 -enable-new-pm=0 -load="$PASS_PATH/ir_instr.so" -instr -S -o "$RESULT/$FILE_NAME" "$RESULT/$FILE_NAME"
    echo
done

