; ModuleID = 'taskwait_opt.bc'
source_filename = "taskwait.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.artsEdtDep_t = type { i64, i32, ptr }

@.str = private unnamed_addr constant [39 x i8] c"** EDT 1: The initial number is %d/%d\0A\00", align 1
@.str.1 = private unnamed_addr constant [31 x i8] c"** EDT 0: The number is %d/%d\0A\00", align 1
@.str.2 = private unnamed_addr constant [31 x i8] c"** EDT 3: The number is %d/%d\0A\00", align 1
@.str.4 = private unnamed_addr constant [31 x i8] c"** EDT 4: The number is %d/%d\0A\00", align 1
@.str.7 = private unnamed_addr constant [37 x i8] c"EDT 2: The final number is %d - %d.\0A\00", align 1
@0 = private unnamed_addr constant [25 x i8] c"Guid for edt_3.task: %u\0A\00", align 1
@1 = private unnamed_addr constant [25 x i8] c"Guid for edt_5.task: %u\0A\00", align 1
@2 = private unnamed_addr constant [25 x i8] c"Guid for edt_2.task: %u\0A\00", align 1
@3 = private unnamed_addr constant [25 x i8] c"Guid for edt_0.sync: %u\0A\00", align 1
@4 = private unnamed_addr constant [25 x i8] c"Guid for edt_4.sync: %u\0A\00", align 1
@5 = private unnamed_addr constant [25 x i8] c"Guid for edt_1.main: %u\0A\00", align 1
@6 = private unnamed_addr constant [25 x i8] c"Guid for edt_6.task: %u\0A\00", align 1
@7 = private unnamed_addr constant [107 x i8] c"Signaling db.shared_number with guid: %u and Value %d, and Address %p from EDT #3 to EDT #6 with guid: %u\0A\00", align 1
@8 = private unnamed_addr constant [107 x i8] c"Signaling db.random_number with guid: %u and Value %d, and Address %p from EDT #5 to EDT #2 with guid: %u\0A\00", align 1
@9 = private unnamed_addr constant [107 x i8] c"Signaling db.random_number with guid: %u and Value %d, and Address %p from EDT #0 to EDT #4 with guid: %u\0A\00", align 1
@10 = private unnamed_addr constant [107 x i8] c"Signaling db.shared_number with guid: %u and Value %d, and Address %p from EDT #0 to EDT #4 with guid: %u\0A\00", align 1
@11 = private unnamed_addr constant [107 x i8] c"Signaling db.random_number with guid: %u and Value %d, and Address %p from EDT #4 to EDT #6 with guid: %u\0A\00", align 1
@12 = private unnamed_addr constant [107 x i8] c"Signaling db.shared_number with guid: %u and Value %d, and Address %p from EDT #4 to EDT #3 with guid: %u\0A\00", align 1
@13 = private unnamed_addr constant [107 x i8] c"Signaling db.random_number with guid: %u and Value %d, and Address %p from EDT #1 to EDT #0 with guid: %u\0A\00", align 1
@14 = private unnamed_addr constant [107 x i8] c"Signaling db.shared_number with guid: %u and Value %d, and Address %p from EDT #1 to EDT #0 with guid: %u\0A\00", align 1
@15 = private unnamed_addr constant [107 x i8] c"Signaling db.random_number with guid: %u and Value %d, and Address %p from EDT #6 to EDT #5 with guid: %u\0A\00", align 1
@16 = private unnamed_addr constant [107 x i8] c"Signaling db.shared_number with guid: %u and Value %d, and Address %p from EDT #6 to EDT #2 with guid: %u\0A\00", align 1
@str = private unnamed_addr constant [16 x i8] c"Creating EDT #3\00", align 1
@str.1 = private unnamed_addr constant [16 x i8] c"Creating EDT #5\00", align 1
@str.2 = private unnamed_addr constant [16 x i8] c"Creating EDT #0\00", align 1
@str.3 = private unnamed_addr constant [16 x i8] c"Creating EDT #2\00", align 1
@str.4 = private unnamed_addr constant [16 x i8] c"Creating EDT #4\00", align 1
@str.5 = private unnamed_addr constant [16 x i8] c"Creating EDT #6\00", align 1
@str.6 = private unnamed_addr constant [16 x i8] c"Creating EDT #1\00", align 1

; Function Attrs: nounwind
declare void @srand(i32 noundef) local_unnamed_addr #0

; Function Attrs: nounwind
declare i64 @time(ptr noundef) local_unnamed_addr #0

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #0

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #1

define internal void @edt_3.task(i32 %paramc, ptr nocapture readonly %paramv, i32 %depc, ptr nocapture readonly %depv) {
entry:
  %0 = load i64, ptr %paramv, align 8
  %1 = trunc i64 %0 to i32
  %edt_3.paramv_1.guid.edt_6 = getelementptr inbounds i64, ptr %paramv, i64 1
  %2 = load i64, ptr %edt_3.paramv_1.guid.edt_6, align 8
  %edt_3.depv_0.guid.load = load i64, ptr %depv, align 8
  %edt_3.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i64 0, i32 2
  %edt_3.depv_0.ptr.load = load ptr, ptr %edt_3.depv_0.ptr, align 8
  %3 = load i32, ptr %edt_3.depv_0.ptr.load, align 4, !tbaa !6, !noalias !10
  %inc.i = add nsw i32 %3, 1
  store i32 %inc.i, ptr %edt_3.depv_0.ptr.load, align 4, !tbaa !6, !noalias !10
  %inc1.i = add nsw i32 %1, 1
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %inc.i, i32 noundef %inc1.i) #4, !noalias !10
  %4 = load i32, ptr %edt_3.depv_0.ptr.load, align 4
  %5 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @7, i64 %edt_3.depv_0.guid.load, i32 %4, ptr nonnull %edt_3.depv_0.ptr.load, i64 %2)
  tail call void @artsSignalEdt(i64 %2, i32 1, i64 %edt_3.depv_0.guid.load)
  ret void
}

define internal void @edt_4.sync(i32 %paramc, ptr nocapture readonly %paramv, i32 %depc, ptr nocapture readonly %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %1 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @0, i64 %0)
  %2 = load i64, ptr %paramv, align 8
  %edt_4.depv_0.guid.load = load i64, ptr %depv, align 8
  %edt_4.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i64 0, i32 2
  %edt_4.depv_0.ptr.load = load ptr, ptr %edt_4.depv_0.ptr, align 8
  %edt_4.depv_1.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i64 1, i32 0
  %edt_4.depv_1.guid.load = load i64, ptr %edt_4.depv_1.guid, align 8
  %edt_4.depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i64 1, i32 2
  %edt_4.depv_1.ptr.load = load ptr, ptr %edt_4.depv_1.ptr, align 8
  %3 = load i32, ptr %edt_4.depv_0.ptr.load, align 4, !tbaa !6
  %4 = load i32, ptr %edt_4.depv_1.ptr.load, align 4, !tbaa !6
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %3, i32 noundef %4) #4
  %5 = load i32, ptr %edt_4.depv_1.ptr.load, align 4, !tbaa !6
  %edt_3_paramv1 = alloca [2 x i64], align 8
  %6 = sext i32 %5 to i64
  store i64 %6, ptr %edt_3_paramv1, align 8
  %edt_3.paramv_1.guid.edt_6 = getelementptr inbounds i64, ptr %edt_3_paramv1, i64 1
  store i64 %2, ptr %edt_3.paramv_1.guid.edt_6, align 8
  %puts = tail call i32 @puts(ptr nonnull dereferenceable(1) @str)
  %7 = call i64 @artsEdtCreateWithGuid(ptr nonnull @edt_3.task, i64 %0, i32 2, ptr nonnull %edt_3_paramv1, i32 1)
  %8 = load i32, ptr %edt_4.depv_1.ptr.load, align 4
  %9 = call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @11, i64 %edt_4.depv_1.guid.load, i32 %8, ptr nonnull %edt_4.depv_1.ptr.load, i64 %2)
  call void @artsSignalEdt(i64 %2, i32 0, i64 %edt_4.depv_1.guid.load)
  %10 = load i32, ptr %edt_4.depv_0.ptr.load, align 4
  %11 = call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @12, i64 %edt_4.depv_0.guid.load, i32 %10, ptr nonnull %edt_4.depv_0.ptr.load, i64 %0)
  call void @artsSignalEdt(i64 %0, i32 0, i64 %edt_4.depv_0.guid.load)
  ret void
}

declare i64 @artsReserveGuidRoute(i32, i32) local_unnamed_addr

define internal void @edt_5.task(i32 %paramc, ptr nocapture readonly %paramv, i32 %depc, ptr nocapture readonly %depv) {
entry:
  %0 = load i64, ptr %paramv, align 8
  %1 = trunc i64 %0 to i32
  %edt_5.paramv_1.guid.edt_2 = getelementptr inbounds i64, ptr %paramv, i64 1
  %2 = load i64, ptr %edt_5.paramv_1.guid.edt_2, align 8
  %edt_5.depv_0.guid.load = load i64, ptr %depv, align 8
  %edt_5.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i64 0, i32 2
  %edt_5.depv_0.ptr.load = load ptr, ptr %edt_5.depv_0.ptr, align 8
  %inc.i = add nsw i32 %1, 1
  %3 = load i32, ptr %edt_5.depv_0.ptr.load, align 4, !tbaa !6, !noalias !13
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.4, i32 noundef %inc.i, i32 noundef %3) #4, !noalias !13
  %4 = load i32, ptr %edt_5.depv_0.ptr.load, align 4
  %5 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @8, i64 %edt_5.depv_0.guid.load, i32 %4, ptr nonnull %edt_5.depv_0.ptr.load, i64 %2)
  tail call void @artsSignalEdt(i64 %2, i32 1, i64 %edt_5.depv_0.guid.load)
  ret void
}

define internal void @edt_6.task(i32 %paramc, ptr nocapture readonly %paramv, i32 %depc, ptr nocapture readonly %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %1 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @1, i64 %0)
  %2 = load i64, ptr %paramv, align 8
  %edt_6.depv_0.guid.load = load i64, ptr %depv, align 8
  %edt_6.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i64 0, i32 2
  %edt_6.depv_0.ptr.load = load ptr, ptr %edt_6.depv_0.ptr, align 8
  %edt_6.depv_1.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i64 1, i32 0
  %edt_6.depv_1.guid.load = load i64, ptr %edt_6.depv_1.guid, align 8
  %edt_6.depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i64 1, i32 2
  %edt_6.depv_1.ptr.load = load ptr, ptr %edt_6.depv_1.ptr, align 8
  %3 = load i32, ptr %edt_6.depv_1.ptr.load, align 4, !tbaa !6
  %edt_5_paramv1 = alloca [2 x i64], align 8
  %4 = sext i32 %3 to i64
  store i64 %4, ptr %edt_5_paramv1, align 8
  %edt_5.paramv_1.guid.edt_2 = getelementptr inbounds i64, ptr %edt_5_paramv1, i64 1
  store i64 %2, ptr %edt_5.paramv_1.guid.edt_2, align 8
  %puts = tail call i32 @puts(ptr nonnull dereferenceable(1) @str.1)
  %5 = call i64 @artsEdtCreateWithGuid(ptr nonnull @edt_5.task, i64 %0, i32 2, ptr nonnull %edt_5_paramv1, i32 1)
  %6 = load i32, ptr %edt_6.depv_0.ptr.load, align 4
  %7 = call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @15, i64 %edt_6.depv_0.guid.load, i32 %6, ptr nonnull %edt_6.depv_0.ptr.load, i64 %0)
  call void @artsSignalEdt(i64 %0, i32 0, i64 %edt_6.depv_0.guid.load)
  %8 = load i32, ptr %edt_6.depv_1.ptr.load, align 4
  %9 = call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @16, i64 %edt_6.depv_1.guid.load, i32 %8, ptr nonnull %edt_6.depv_1.ptr.load, i64 %2)
  call void @artsSignalEdt(i64 %2, i32 0, i64 %edt_6.depv_1.guid.load)
  ret void
}

define internal void @edt_2.task(i32 %paramc, ptr nocapture readnone %paramv, i32 %depc, ptr nocapture readonly %depv) {
entry:
  %edt_2.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i64 0, i32 2
  %edt_2.depv_0.ptr.load = load ptr, ptr %edt_2.depv_0.ptr, align 8
  %edt_2.depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i64 1, i32 2
  %edt_2.depv_1.ptr.load = load ptr, ptr %edt_2.depv_1.ptr, align 8
  %0 = load i32, ptr %edt_2.depv_0.ptr.load, align 4, !tbaa !6
  %1 = load i32, ptr %edt_2.depv_1.ptr.load, align 4, !tbaa !6
  %call6 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.7, i32 noundef %0, i32 noundef %1) #4
  tail call void @artsShutdown()
  ret void
}

define internal void @edt_1.main(i32 %paramc, ptr nocapture readnone %paramv, i32 %depc, ptr nocapture readnone %depv) {
entry:
  %edt_2_paramv2 = alloca [0 x i64], align 8
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %1 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @2, i64 %0)
  %2 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %3 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @3, i64 %2)
  %db.random_number.addr = alloca i64, align 8
  %4 = call i64 @artsDbCreatePtr(ptr nonnull %db.random_number.addr, i64 4, i32 7)
  %db.random_number.addr.ld = load i64, ptr %db.random_number.addr, align 8
  %db.random_number.ptr = inttoptr i64 %db.random_number.addr.ld to ptr
  %db.shared_number.addr = alloca i64, align 8
  %5 = call i64 @artsDbCreatePtr(ptr nonnull %db.shared_number.addr, i64 4, i32 7)
  %db.shared_number.addr.ld = load i64, ptr %db.shared_number.addr, align 8
  %db.shared_number.ptr = inttoptr i64 %db.shared_number.addr.ld to ptr
  %call = tail call i64 @time(ptr noundef null) #4
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #4
  %call1 = tail call i32 @rand() #4
  %rem = srem i32 %call1, 100
  %add = add nsw i32 %rem, 1
  store i32 %add, ptr %db.shared_number.ptr, align 4, !tbaa !6
  %call2 = tail call i32 @rand() #4
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %db.random_number.ptr, align 4, !tbaa !6
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %add, i32 noundef %add4) #4
  %edt_0_paramv = alloca i64, align 8
  store i64 %0, ptr %edt_0_paramv, align 8
  %puts = call i32 @puts(ptr nonnull dereferenceable(1) @str.2)
  %6 = call i64 @artsEdtCreateWithGuid(ptr nonnull @edt_0.sync, i64 %2, i32 1, ptr nonnull %edt_0_paramv, i32 2)
  %puts3 = call i32 @puts(ptr nonnull dereferenceable(1) @str.3)
  %7 = call i64 @artsEdtCreateWithGuid(ptr nonnull @edt_2.task, i64 %0, i32 0, ptr nonnull %edt_2_paramv2, i32 2)
  %8 = load i32, ptr %db.random_number.ptr, align 4
  %9 = call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @13, i64 %4, i32 %8, ptr nonnull %db.random_number.ptr, i64 %2)
  call void @artsSignalEdt(i64 %2, i32 1, i64 %4)
  %10 = load i32, ptr %db.shared_number.ptr, align 4
  %11 = call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @14, i64 %5, i32 %10, ptr nonnull %db.shared_number.ptr, i64 %2)
  call void @artsSignalEdt(i64 %2, i32 0, i64 %5)
  ret void
}

define internal void @edt_0.sync(i32 %paramc, ptr nocapture readonly %paramv, i32 %depc, ptr nocapture readonly %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %1 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @4, i64 %0)
  %2 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %3 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @6, i64 %2)
  %4 = load i64, ptr %paramv, align 8
  %edt_0.depv_0.guid.load = load i64, ptr %depv, align 8
  %edt_0.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i64 0, i32 2
  %edt_0.depv_0.ptr.load = load ptr, ptr %edt_0.depv_0.ptr, align 8
  %edt_0.depv_1.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i64 1, i32 0
  %edt_0.depv_1.guid.load = load i64, ptr %edt_0.depv_1.guid, align 8
  %edt_0.depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i64 1, i32 2
  %edt_0.depv_1.ptr.load = load ptr, ptr %edt_0.depv_1.ptr, align 8
  %edt_4_paramv = alloca i64, align 8
  store i64 %2, ptr %edt_4_paramv, align 8
  %puts = tail call i32 @puts(ptr nonnull dereferenceable(1) @str.4)
  %5 = call i64 @artsEdtCreateWithGuid(ptr nonnull @edt_4.sync, i64 %0, i32 1, ptr nonnull %edt_4_paramv, i32 2)
  %edt_6_paramv = alloca i64, align 8
  store i64 %4, ptr %edt_6_paramv, align 8
  %puts2 = call i32 @puts(ptr nonnull dereferenceable(1) @str.5)
  %6 = call i64 @artsEdtCreateWithGuid(ptr nonnull @edt_6.task, i64 %2, i32 1, ptr nonnull %edt_6_paramv, i32 2)
  %7 = load i32, ptr %edt_0.depv_1.ptr.load, align 4
  %8 = call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @9, i64 %edt_0.depv_1.guid.load, i32 %7, ptr nonnull %edt_0.depv_1.ptr.load, i64 %0)
  call void @artsSignalEdt(i64 %0, i32 1, i64 %edt_0.depv_1.guid.load)
  %9 = load i32, ptr %edt_0.depv_0.ptr.load, align 4
  %10 = call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @10, i64 %edt_0.depv_0.guid.load, i32 %9, ptr nonnull %edt_0.depv_0.ptr.load, i64 %0)
  call void @artsSignalEdt(i64 %0, i32 0, i64 %edt_0.depv_0.guid.load)
  ret void
}

declare i64 @artsDbCreatePtr(ptr, i64, i32) local_unnamed_addr

declare i64 @artsEdtCreateWithGuid(ptr, i64, i32, ptr, i32) local_unnamed_addr

declare void @artsSignalEdt(i64, i32, i64) local_unnamed_addr

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none)
define dso_local void @initPerNode(i32 %nodeId, i32 %argc, ptr nocapture readnone %argv) local_unnamed_addr #2 {
entry:
  ret void
}

define dso_local void @initPerWorker(i32 %nodeId, i32 %workerId, i32 %argc, ptr nocapture readnone %argv) local_unnamed_addr {
entry:
  %edt_1_paramv1 = alloca [0 x i64], align 8
  %0 = or i32 %workerId, %nodeId
  %1 = icmp eq i32 %0, 0
  br i1 %1, label %then, label %exit

then:                                             ; preds = %entry
  %2 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %3 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @5, i64 %2)
  %puts = tail call i32 @puts(ptr nonnull dereferenceable(1) @str.6)
  %4 = call i64 @artsEdtCreateWithGuid(ptr nonnull @edt_1.main, i64 %2, i32 0, ptr nonnull %edt_1_paramv1, i32 0)
  br label %exit

exit:                                             ; preds = %then, %entry
  ret void
}

define dso_local noundef i32 @main(i32 %argc, ptr %argv) local_unnamed_addr {
entry:
  tail call void @artsRT(i32 %argc, ptr %argv)
  ret i32 0
}

declare void @artsRT(i32, ptr) local_unnamed_addr

declare void @artsShutdown() local_unnamed_addr

; Function Attrs: nofree nounwind
declare noundef i32 @puts(ptr nocapture noundef readonly) local_unnamed_addr #3

attributes #0 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { mustprogress nofree norecurse nosync nounwind willreturn memory(none) }
attributes #3 = { nofree nounwind }
attributes #4 = { nounwind }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 51}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 7, !"uwtable", i32 2}
!5 = !{!"clang version 18.1.8"}
!6 = !{!7, !7, i64 0}
!7 = !{!"int", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C++ TBAA"}
!10 = !{!11}
!11 = distinct !{!11, !12, !".omp_outlined.: %__context"}
!12 = distinct !{!12, !".omp_outlined."}
!13 = !{!14}
!14 = distinct !{!14, !15, !".omp_outlined..3: %__context"}
!15 = distinct !{!15, !".omp_outlined..3"}
