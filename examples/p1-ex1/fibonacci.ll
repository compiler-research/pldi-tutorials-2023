; Run the program by "clang -o fibonacci fibonacci.ll && ./fibonacci"

@str = constant [3 x i8] c"%d\00"

declare i32 @printf(i8*, i32)

define i32 @fib(i32 %n) {
  %cmp = icmp sle i32 %n, 1
  br i1 %cmp, label %ret_n, label %recursion
ret_n:
  br label %end
recursion:
  %n_1 = sub i32 %n, 1
  %fib_n_1 = call i32 @fib(i32 %n_1)
  %n_2 = sub i32 %n, 2
  %fib_n_2 = call i32 @fib(i32 %n_2)
  %add = add i32 %fib_n_1, %fib_n_2
  br label %end
end:
  %res = phi i32 [ %n, %ret_n ], [ %add, %recursion ]
  ret i32 %res
}

define i32 @main() {
  %res = call i32 @fib(i32 8)
  call i32 @printf(ptr @str, i32 %res)
  ret i32 0
}
