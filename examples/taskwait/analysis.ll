; IR OUTPUT

define internal void @carts.edt() #8 !carts !22 {
entry:
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #6
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #6
  %call1 = tail call i32 @rand() #6
  %rem = srem i32 %call1, 100
  %add = add nsw i32 %rem, 1
  store i32 %add, ptr %shared_number, align 4, !tbaa !13
  %call2 = tail call i32 @rand() #6
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %random_number, align 4, !tbaa !13
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %add, i32 noundef %add4) #6
  call void @carts.edt.1(ptr nocapture %shared_number, ptr nocapture %random_number) #11
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  call void @carts.edt.6(ptr nocapture %shared_number, ptr nocapture %random_number) #6
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret void
}

define internal void @carts.edt.1(ptr nocapture %0, ptr nocapture readonly %1) #4 !carts !6 {
entry:
  br label %codeRepl1

codeRepl1:                                        ; preds = %entry
  call void @carts.edt.5(ptr nocapture %0, ptr nocapture readonly %1) #6
  br label %codeRepl

codeRepl:                                         ; preds = %codeRepl1
  call void @carts.edt.3(ptr nocapture readonly %1, ptr nocapture %0)
  br label %exit.ret

exit.ret:                                         ; preds = %codeRepl
  ret void
}

define internal void @carts.edt.2(ptr nocapture %0, i32 %1) #4 !carts !8 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !10)
  %2 = load i32, ptr %0, align 4, !tbaa !13, !noalias !10
  %inc.i = add nsw i32 %2, 1
  store i32 %inc.i, ptr %0, align 4, !tbaa !13, !noalias !10
  %inc1.i = add nsw i32 %1, 1
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc.i, i32 noundef %inc1.i) #6, !noalias !10
  ret void
}

define internal void @carts.edt.3(ptr nocapture readonly %0, ptr nocapture readonly %1) #9 !carts !24 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %2 = load i32, ptr %1, align 4, !tbaa !13
  call void @carts.edt.4(ptr nocapture readonly %0, i32 %2) #11
  br label %exit

exit:                                             ; preds = %entry.split
  br label %exit.ret.exitStub

exit.ret.exitStub:                                ; preds = %exit
  ret void
}

define internal void @carts.edt.4(ptr nocapture readonly %0, i32 %1) #4 !carts !8 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !17)
  %inc.i = add nsw i32 %1, 1
  %2 = load i32, ptr %0, align 4, !tbaa !13, !noalias !17
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.4, i32 noundef %inc.i, i32 noundef %2) #6, !noalias !17
  ret void
}

define internal void @carts.edt.5(ptr nocapture %0, ptr nocapture readonly %1) #10 !carts !6 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %2 = load i32, ptr %0, align 4, !tbaa !13
  %3 = load i32, ptr %1, align 4, !tbaa !13
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %2, i32 noundef %3) #6
  %4 = load i32, ptr %1, align 4, !tbaa !13
  call void @carts.edt.2(ptr nocapture %0, i32 %4) #12
  br label %codeRepl.exitStub

codeRepl.exitStub:                                ; preds = %entry.split
  ret void
}

; Function Attrs: norecurse nounwind
define internal void @carts.edt.6(ptr nocapture readonly %shared_number, ptr nocapture readonly %random_number) #8 !carts !24 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %0 = load i32, ptr %shared_number, align 4, !tbaa !13
  %1 = load i32, ptr %random_number, align 4, !tbaa !13
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.7, i32 noundef %0, i32 noundef %1) #6
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  ret void
}
