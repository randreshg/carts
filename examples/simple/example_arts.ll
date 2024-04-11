; ModuleID = 'example_arts.cpp'
source_filename = "example_arts.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [27 x i8] c"I think the number is %d.\0A\00", align 1
@.str.1 = private unnamed_addr constant [31 x i8] c"The final number is %d - % d.\0A\00", align 1
@.str.2 = private unnamed_addr constant [17 x i8] c"Node %u argc %u\0A\00", align 1
@.str.3 = private unnamed_addr constant [25 x i8] c"NodeID %u - WorkerID %u\0A\00", align 1

; Function Attrs: mustprogress uwtable
define dso_local void @_Z29artsParallelEdtCreateWithGuidPFvjPmjP12artsEdtDep_tEjS_jj(ptr noundef %funcPtr, i32 noundef %paramc, ptr noundef %paramv, i32 noundef %depc, i32 noundef %route) local_unnamed_addr #0 {
entry:
  %call4 = tail call i32 @artsGetTotalWorkers()
  %cmp5.not = icmp eq i32 %call4, 0
  br i1 %cmp5.not, label %for.cond.cleanup, label %for.body

for.cond.cleanup:                                 ; preds = %for.body, %entry
  ret void

for.body:                                         ; preds = %entry, %for.body
  %i.06 = phi i32 [ %inc, %for.body ], [ 0, %entry ]
  %call1 = tail call i64 @artsReserveGuidRoute(i32 noundef 1, i32 noundef %route)
  %call2 = tail call i64 @artsEdtCreateWithGuid(ptr noundef %funcPtr, i64 noundef %call1, i32 noundef %paramc, ptr noundef %paramv, i32 noundef %depc)
  %inc = add nuw i32 %i.06, 1
  %call = tail call i32 @artsGetTotalWorkers()
  %cmp = icmp ult i32 %inc, %call
  br i1 %cmp, label %for.body, label %for.cond.cleanup, !llvm.loop !5
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

declare i32 @artsGetTotalWorkers() local_unnamed_addr #2

declare i64 @artsReserveGuidRoute(i32 noundef, i32 noundef) local_unnamed_addr #2

declare i64 @artsEdtCreateWithGuid(ptr noundef, i64 noundef, i32 noundef, ptr noundef, i32 noundef) local_unnamed_addr #2

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: mustprogress nofree nounwind uwtable
define dso_local void @_Z8task_edtjPmjP12artsEdtDep_t(i32 %paramc, ptr nocapture noundef readonly %paramv, i32 %depc, ptr nocapture readnone %depv) #3 {
entry:
  %0 = load i64, ptr %paramv, align 8, !tbaa !7
  %conv = trunc i64 %0 to i32
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %conv)
  ret void
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #4

; Function Attrs: mustprogress uwtable
define dso_local void @_Z12parallel_edtjPmjP12artsEdtDep_t(i32 %paramc, ptr nocapture noundef readonly %paramv, i32 %depc, ptr nocapture readnone %depv) #0 {
entry:
  %task_paramv = alloca [1 x i64], align 8
  %0 = load i64, ptr %paramv, align 8, !tbaa !7
  %call = tail call i64 @artsReserveGuidRoute(i32 noundef 1, i32 noundef 0)
  call void @llvm.lifetime.start.p0(i64 8, ptr nonnull %task_paramv) #7
  %conv1 = and i64 %0, 4294967295
  store i64 %conv1, ptr %task_paramv, align 8, !tbaa !7
  %call2 = call i64 @artsEdtCreateWithGuid(ptr noundef nonnull @_Z8task_edtjPmjP12artsEdtDep_t, i64 noundef %call, i32 noundef 1, ptr noundef nonnull %task_paramv, i32 noundef 0)
  call void @llvm.lifetime.end.p0(i64 8, ptr nonnull %task_paramv) #7
  ret void
}

; Function Attrs: mustprogress uwtable
define dso_local void @_Z8main_edtjPmjP12artsEdtDep_t(i32 %paramc, ptr nocapture readnone %paramv, i32 %depc, ptr nocapture readnone %depv) #0 {
entry:
  %parallel_paramv = alloca [1 x i64], align 8
  %call = tail call i32 @rand() #7
  call void @llvm.lifetime.start.p0(i64 8, ptr nonnull %parallel_paramv) #7
  store i64 1, ptr %parallel_paramv, align 8, !tbaa !7
  %call4.i = tail call i32 @artsGetTotalWorkers()
  %cmp5.not.i = icmp eq i32 %call4.i, 0
  br i1 %cmp5.not.i, label %_Z29artsParallelEdtCreateWithGuidPFvjPmjP12artsEdtDep_tEjS_jj.exit, label %for.body.i

for.body.i:                                       ; preds = %entry, %for.body.i
  %i.06.i = phi i32 [ %inc.i, %for.body.i ], [ 0, %entry ]
  %call1.i = call i64 @artsReserveGuidRoute(i32 noundef 1, i32 noundef 0)
  %call2.i = call i64 @artsEdtCreateWithGuid(ptr noundef nonnull @_Z12parallel_edtjPmjP12artsEdtDep_t, i64 noundef %call1.i, i32 noundef 1, ptr noundef nonnull %parallel_paramv, i32 noundef 0)
  %inc.i = add nuw i32 %i.06.i, 1
  %call.i = call i32 @artsGetTotalWorkers()
  %cmp.i = icmp ult i32 %inc.i, %call.i
  br i1 %cmp.i, label %for.body.i, label %_Z29artsParallelEdtCreateWithGuidPFvjPmjP12artsEdtDep_tEjS_jj.exit, !llvm.loop !5

_Z29artsParallelEdtCreateWithGuidPFvjPmjP12artsEdtDep_tEjS_jj.exit: ; preds = %for.body.i, %entry
  %rem = srem i32 %call, 10
  %add = add nsw i32 %rem, 10
  %call1 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef 1, i32 noundef %add)
  call void @llvm.lifetime.end.p0(i64 8, ptr nonnull %parallel_paramv) #7
  ret void
}

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #5

; Function Attrs: mustprogress uwtable
define dso_local void @_Z11initPerNodejiPPc(i32 noundef %nodeId, i32 noundef %argc, ptr nocapture noundef readnone %argv) local_unnamed_addr #0 {
entry:
  tail call void (ptr, ...) @PRINTF(ptr noundef nonnull @.str.2, i32 noundef %nodeId, i32 noundef %argc)
  ret void
}

declare void @PRINTF(ptr noundef, ...) local_unnamed_addr #2

; Function Attrs: mustprogress uwtable
define dso_local void @_Z13initPerWorkerjjiPPc(i32 noundef %nodeId, i32 noundef %workerId, i32 noundef %argc, ptr nocapture noundef readnone %argv) local_unnamed_addr #0 {
entry:
  %0 = or i32 %workerId, %nodeId
  %or.cond.not = icmp eq i32 %0, 0
  br i1 %or.cond.not, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  tail call void (ptr, ...) @PRINTF(ptr noundef nonnull @.str.3, i32 noundef 0, i32 noundef 0)
  %call = tail call i64 @artsEdtCreate(ptr noundef nonnull @_Z8main_edtjPmjP12artsEdtDep_t, i32 noundef 0, i32 noundef 0, ptr noundef undef, i32 noundef 0)
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  ret void
}

declare i64 @artsEdtCreate(ptr noundef, i32 noundef, i32 noundef, ptr noundef, i32 noundef) local_unnamed_addr #2

; Function Attrs: mustprogress norecurse uwtable
define dso_local noundef i32 @main(i32 noundef %argc, ptr noundef %argv) local_unnamed_addr #6 {
entry:
  %call = tail call i32 @artsRT(i32 noundef %argc, ptr noundef %argv)
  ret i32 0
}

declare i32 @artsRT(i32 noundef, ptr noundef) local_unnamed_addr #2

attributes #0 = { mustprogress uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { mustprogress nofree nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { mustprogress norecurse uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #7 = { nounwind }

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{!"clang version 18.0.0"}
!5 = distinct !{!5, !6}
!6 = !{!"llvm.loop.mustprogress"}
!7 = !{!8, !8, i64 0}
!8 = !{!"long", !9, i64 0}
!9 = !{!"omnipotent char", !10, i64 0}
!10 = !{!"Simple C++ TBAA"}
