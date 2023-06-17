; Run the program by "clang -o main main.ll gv.ll && ./main"

@gv = external global i32
define void @storeToGlobalVariable() {
  store i32 53, ptr @gv
  ret void
}

@str = constant [4 x i8] c"%d\0A\00"

declare i32 @printf(i8*, i32)

define i32 @main() {
  %1 = load i32, ptr @gv
  call i32 @printf(ptr @str, i32 %1)
  call void @storeToGlobalVariable()
  %3 = load i32, ptr @gv
  call i32 @printf(ptr @str, i32 %3)
  ret i32 0
}
