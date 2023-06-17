; Run the program by "clang -o main main.ll && ./main"

@str = constant [13 x i8] c"Hello, world\00"

declare i32 @printf(i8*)

define i32 @main() {
  call i32 @printf(ptr @str)
  ret i32 0
}
