; Run the program by "clang -o triangular triangular.ll && ./triangular"

@str = constant [3 x i8] c"%d\00"

declare i32 @printf(i8*, i32)

define i32 @triangularNumber(i32 %n) {
  %add = add i32 %n, 1
  %mul = mul i32 %n, %add
  %res = sdiv i32 %mul, 2
  ret i32 %res
}

define i32 @main() {
  %res = call i32 @triangularNumber(i32 8)
  call i32 @printf(ptr @str, i32 %res)
  ret i32 0
}
