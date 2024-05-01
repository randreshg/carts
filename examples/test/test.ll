; ModuleID = 'test.bc'
source_filename = "test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [7 x i8] c"Start\0A\00", align 1
@.str.1 = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1
@.str.2 = private unnamed_addr constant [15 x i8] c"  Result = %d\0A\00", align 1
@.str.3 = private unnamed_addr constant [5 x i8] c"End\0A\00", align 1

; Function Attrs: nounwind uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32*, align 8
  %3 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %4 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([7 x i8], [7 x i8]* @.str, i64 0, i64 0))
  %5 = bitcast i32** %2 to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %5) #4
  %6 = call noalias i8* @malloc(i64 noundef 4) #4
  %7 = bitcast i8* %6 to i32*
  store i32* %7, i32** %2, align 8, !tbaa !3
  %8 = load i32*, i32** %2, align 8, !tbaa !3
  store i32 0, i32* %8, align 4, !tbaa !7
  %9 = bitcast i32* %3 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %9) #4
  store i32 0, i32* %3, align 4, !tbaa !7
  br label %10

10:                                               ; preds = %19, %0
  %11 = load i32, i32* %3, align 4, !tbaa !7
  %12 = icmp slt i32 %11, 10
  br i1 %12, label %15, label %13

13:                                               ; preds = %10
  %14 = bitcast i32* %3 to i8*
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %14) #4
  br label %22

15:                                               ; preds = %10
  %16 = load i32*, i32** %2, align 8, !tbaa !3
  store i32 1, i32* %16, align 4, !tbaa !7
  %17 = load i32, i32* %3, align 4, !tbaa !7
  %18 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([4 x i8], [4 x i8]* @.str.1, i64 0, i64 0), i32 noundef %17)
  br label %19

19:                                               ; preds = %15
  %20 = load i32, i32* %3, align 4, !tbaa !7
  %21 = add nsw i32 %20, 1
  store i32 %21, i32* %3, align 4, !tbaa !7
  br label %10, !llvm.loop !9

22:                                               ; preds = %13
  %23 = load i32*, i32** %2, align 8, !tbaa !3
  %24 = load i32, i32* %23, align 4, !tbaa !7
  %25 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([15 x i8], [15 x i8]* @.str.2, i64 0, i64 0), i32 noundef %24)
  %26 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([5 x i8], [5 x i8]* @.str.3, i64 0, i64 0))
  %27 = bitcast i32** %2 to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %27) #4
  ret i32 0
}

declare dso_local i32 @printf(i8* noundef, ...) #1

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: nounwind
declare dso_local noalias i8* @malloc(i64 noundef) #3

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #2

attributes #0 = { nounwind uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { argmemonly nofree nosync nounwind willreturn }
attributes #3 = { nounwind "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"uwtable", i32 1}
!2 = !{!"clang version 14.0.1"}
!3 = !{!4, !4, i64 0}
!4 = !{!"any pointer", !5, i64 0}
!5 = !{!"omnipotent char", !6, i64 0}
!6 = !{!"Simple C/C++ TBAA"}
!7 = !{!8, !8, i64 0}
!8 = !{!"int", !5, i64 0}
!9 = distinct !{!9, !10, !11}
!10 = !{!"llvm.loop.mustprogress"}
!11 = !{!"llvm.loop.unroll.disable"}
