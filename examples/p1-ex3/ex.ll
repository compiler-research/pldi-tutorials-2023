@mod = global i32 998244353
define i32 @modadd(i32 %a, i32 %b) {
  %add = add i32 %a, %b
  %mod = load i32, ptr @mod
  %res = srem i32 %add, %mod
  ret i32 %res
}