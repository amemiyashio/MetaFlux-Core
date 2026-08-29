; Regression for llvm/llvm-project#186922.
; The two distinct GEPs resolve to the same loop-invariant address. LAA must
; reject vectorization instead of treating the pair as runtime-checkable.

define void @same_invariant_address(ptr %source, ptr %destination, ptr %base,
                                    i64 %count) {
entry:
  br label %loop

loop:
  %index = phi i64 [ 0, %entry ], [ %next, %loop ]
  %source.element = getelementptr i32, ptr %source, i64 %index
  %value = load i32, ptr %source.element, align 4
  %store.address = getelementptr i32, ptr %base, i64 1
  store i32 %value, ptr %store.address, align 4
  %load.address = getelementptr i8, ptr %base, i64 4
  %roundtrip = load i32, ptr %load.address, align 4
  %destination.element = getelementptr i32, ptr %destination, i64 %index
  store i32 %roundtrip, ptr %destination.element, align 4
  %next = add nuw i64 %index, 1
  %done = icmp eq i64 %next, %count
  br i1 %done, label %exit, label %loop

exit:
  ret void
}
