; ModuleID = 'fib.bc'
source_filename = "fib.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.artsEdtDep_t = type { i64, i32, ptr }

@start = dso_local local_unnamed_addr global i64 0, align 8
@.str = private unnamed_addr constant [44 x i8] c"Fib %u: %u time: %lu nodes: %u workers: %u\0A\00", align 1

; Function Attrs: nounwind uwtable
define dso_local void @fibJoin(i32 %paramc, ptr nocapture noundef readonly %paramv, i32 %depc, ptr nocapture noundef readonly %depv) #0 {
entry:
  %0 = load i64, ptr %depv, align 8, !tbaa !5
  %arrayidx1 = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i64 1
  %1 = load i64, ptr %arrayidx1, align 8, !tbaa !5
  %2 = load i64, ptr %paramv, align 8, !tbaa !12
  %arrayidx5 = getelementptr inbounds i64, ptr %paramv, i64 1
  %3 = load i64, ptr %arrayidx5, align 8, !tbaa !12
  %conv6 = trunc i64 %3 to i32
  %add = add i64 %1, %0
  %conv7 = and i64 %add, 4294967295
  tail call void @artsSignalEdtValue(i64 noundef %2, i32 noundef %conv6, i64 noundef %conv7) #5
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

declare void @artsSignalEdtValue(i64 noundef, i32 noundef, i64 noundef) local_unnamed_addr #2

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nounwind uwtable
define dso_local void @fibFork(i32 noundef %paramc, ptr noundef %paramv, i32 %depc, ptr nocapture readnone %depv) #0 {
entry:
  %args = alloca [3 x i64], align 16
  %call = tail call i32 (...) @artsGetCurrentNode() #5
  %call1 = tail call i32 (...) @artsGetTotalNodes() #5
  %arrayidx3 = getelementptr inbounds i64, ptr %paramv, i64 2
  %0 = load i64, ptr %arrayidx3, align 8, !tbaa !12
  %1 = and i64 %0, 4294967294
  %cmp = icmp eq i64 %1, 0
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %arrayidx2 = getelementptr inbounds i64, ptr %paramv, i64 1
  %2 = load i64, ptr %arrayidx2, align 8, !tbaa !12
  %conv = trunc i64 %2 to i32
  %3 = load i64, ptr %paramv, align 8, !tbaa !12
  %conv6 = and i64 %0, 1
  tail call void @artsSignalEdtValue(i64 noundef %3, i32 noundef %conv, i64 noundef %conv6) #5
  br label %if.end

if.else:                                          ; preds = %entry
  %add = add i32 %call, 1
  %rem = urem i32 %add, %call1
  %call7 = tail call i32 (...) @artsGetCurrentNode() #5
  %sub = add i32 %paramc, -1
  %call8 = tail call i64 @artsEdtCreate(ptr noundef nonnull @fibJoin, i32 noundef %call7, i32 noundef %sub, ptr noundef nonnull %paramv, i32 noundef 2) #5
  call void @llvm.lifetime.start.p0(i64 24, ptr nonnull %args) #5
  store i64 %call8, ptr %args, align 16, !tbaa !12
  %arrayinit.element = getelementptr inbounds i64, ptr %args, i64 1
  store i64 0, ptr %arrayinit.element, align 8, !tbaa !12
  %arrayinit.element9 = getelementptr inbounds i64, ptr %args, i64 2
  %sub10 = add i64 %0, 4294967295
  %conv11 = and i64 %sub10, 4294967295
  store i64 %conv11, ptr %arrayinit.element9, align 16, !tbaa !12
  %call12 = call i64 @artsEdtCreate(ptr noundef nonnull @fibFork, i32 noundef %rem, i32 noundef 3, ptr noundef nonnull %args, i32 noundef 0) #5
  store i64 1, ptr %arrayinit.element, align 8, !tbaa !12
  %sub14 = add i64 %0, 4294967294
  %conv15 = and i64 %sub14, 4294967295
  store i64 %conv15, ptr %arrayinit.element9, align 16, !tbaa !12
  %call18 = call i64 @artsEdtCreate(ptr noundef nonnull @fibFork, i32 noundef %rem, i32 noundef 3, ptr noundef nonnull %args, i32 noundef 0) #5
  call void @llvm.lifetime.end.p0(i64 24, ptr nonnull %args) #5
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  ret void
}

declare i32 @artsGetCurrentNode(...) local_unnamed_addr #2

declare i32 @artsGetTotalNodes(...) local_unnamed_addr #2

declare i64 @artsEdtCreate(ptr noundef, i32 noundef, i32 noundef, ptr noundef, i32 noundef) local_unnamed_addr #2

; Function Attrs: nounwind uwtable
define dso_local void @fibDone(i32 %paramc, ptr nocapture noundef readonly %paramv, i32 %depc, ptr nocapture noundef readonly %depv) #0 {
entry:
  %call = tail call i64 (...) @artsGetTimeStamp() #5
  %0 = load i64, ptr @start, align 8, !tbaa !12
  %sub = sub i64 %call, %0
  %1 = load i64, ptr %paramv, align 8, !tbaa !12
  %2 = load i64, ptr %depv, align 8, !tbaa !5
  %call2 = tail call i32 (...) @artsGetTotalNodes() #5
  %call3 = tail call i32 (...) @artsGetTotalWorkers() #5
  tail call void (ptr, ...) @PRINTF(ptr noundef nonnull @.str, i64 noundef %1, i64 noundef %2, i64 noundef %sub, i32 noundef %call2, i32 noundef %call3) #5
  tail call void (...) @artsShutdown() #5
  ret void
}

declare i64 @artsGetTimeStamp(...) local_unnamed_addr #2

declare void @PRINTF(ptr noundef, ...) local_unnamed_addr #2

declare i32 @artsGetTotalWorkers(...) local_unnamed_addr #2

declare void @artsShutdown(...) local_unnamed_addr #2

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable
define dso_local void @initPerNode(i32 noundef %nodeId, i32 noundef %argc, ptr nocapture noundef readnone %argv) local_unnamed_addr #3 {
entry:
  ret void
}

; Function Attrs: nounwind uwtable
define dso_local void @initPerWorker(i32 noundef %nodeId, i32 noundef %workerId, i32 noundef %argc, ptr nocapture noundef readonly %argv) local_unnamed_addr #0 {
entry:
  %num = alloca i32, align 4
  %args = alloca [3 x i64], align 16
  %0 = or i32 %workerId, %nodeId
  %or.cond.not = icmp eq i32 %0, 0
  br i1 %or.cond.not, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %num) #5
  %arrayidx = getelementptr inbounds ptr, ptr %argv, i64 1
  %1 = load ptr, ptr %arrayidx, align 8, !tbaa !13
  %call.i = tail call i64 @strtol(ptr nocapture noundef nonnull %1, ptr noundef null, i32 noundef 10) #5
  %conv.i = trunc i64 %call.i to i32
  store i32 %conv.i, ptr %num, align 4, !tbaa !14
  %call2 = tail call i64 @artsReserveGuidRoute(i32 noundef 1, i32 noundef 0) #5
  %call3 = call i64 @artsEdtCreateWithGuid(ptr noundef nonnull @fibDone, i64 noundef %call2, i32 noundef 0, ptr noundef nonnull %num, i32 noundef 1) #5
  call void @llvm.lifetime.start.p0(i64 24, ptr nonnull %args) #5
  store i64 %call2, ptr %args, align 16, !tbaa !12
  %arrayinit.element = getelementptr inbounds i64, ptr %args, i64 1
  store i64 0, ptr %arrayinit.element, align 8, !tbaa !12
  %arrayinit.element4 = getelementptr inbounds i64, ptr %args, i64 2
  %2 = load i32, ptr %num, align 4, !tbaa !14
  %conv = zext i32 %2 to i64
  store i64 %conv, ptr %arrayinit.element4, align 16, !tbaa !12
  %call5 = call i64 (...) @artsGetTimeStamp() #5
  store i64 %call5, ptr @start, align 8, !tbaa !12
  %call6 = call i64 @artsEdtCreate(ptr noundef nonnull @fibFork, i32 noundef 0, i32 noundef 3, ptr noundef nonnull %args, i32 noundef 0) #5
  call void @llvm.lifetime.end.p0(i64 24, ptr nonnull %args) #5
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %num) #5
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  ret void
}

declare i64 @artsReserveGuidRoute(i32 noundef, i32 noundef) local_unnamed_addr #2

declare i64 @artsEdtCreateWithGuid(ptr noundef, i64 noundef, i32 noundef, ptr noundef, i32 noundef) local_unnamed_addr #2

; Function Attrs: nounwind uwtable
define dso_local noundef i32 @main(i32 noundef %argc, ptr noundef %argv) local_unnamed_addr #0 {
entry:
  %call = tail call i32 @artsRT(i32 noundef %argc, ptr noundef %argv) #5
  ret i32 0
}

declare i32 @artsRT(i32 noundef, ptr noundef) local_unnamed_addr #2

; Function Attrs: mustprogress nofree nounwind willreturn
declare i64 @strtol(ptr noundef readonly, ptr nocapture noundef, i32 noundef) local_unnamed_addr #4

attributes #0 = { nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { mustprogress nofree nounwind willreturn "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { nounwind }

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{!"clang version 18.1.8"}
!5 = !{!6, !7, i64 0}
!6 = !{!"", !7, i64 0, !10, i64 8, !11, i64 16}
!7 = !{!"long", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C/C++ TBAA"}
!10 = !{!"int", !8, i64 0}
!11 = !{!"any pointer", !8, i64 0}
!12 = !{!7, !7, i64 0}
!13 = !{!11, !11, i64 0}
!14 = !{!10, !10, i64 0}
