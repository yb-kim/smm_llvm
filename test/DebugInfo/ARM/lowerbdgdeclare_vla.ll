; RUN: opt  -instcombine %s -S | FileCheck %s
;
; Generate me from:
; clang -cc1 -triple thumbv7-apple-ios7.0.0 -S -target-abi apcs-gnu -gdwarf-2 -Os test.c -o test.ll -emit-llvm
; void run(float r)
; {
;   int count = r;
;   float vla[count];
;   vla[0] = r;
;   for (int i = 0; i < count; i++)
;     vla[i] /= r;
; }
; rdar://problem/15464571
;
; ModuleID = 'test.c'
target datalayout = "e-p:32:32:32-i1:8:32-i8:8:32-i16:16:32-i32:32:32-i64:32:64-f32:32:32-f64:32:64-v64:32:64-v128:32:128-a0:0:32-n32-S32"
target triple = "thumbv7-apple-ios8.0.0"

; Function Attrs: nounwind optsize readnone
define void @run(float %r) #0 {
entry:
  tail call void @llvm.dbg.declare(metadata !{float %r}, metadata !11, metadata !{metadata !"0x102"}), !dbg !22
  %conv = fptosi float %r to i32, !dbg !23
  tail call void @llvm.dbg.declare(metadata !{i32 %conv}, metadata !12, metadata !{metadata !"0x102"}), !dbg !23
  %vla = alloca float, i32 %conv, align 4, !dbg !24
  tail call void @llvm.dbg.declare(metadata !{float* %vla}, metadata !14, metadata !{metadata !"0x102"}), !dbg !24
; The VLA alloca should be described by a dbg.declare:
; CHECK: call void @llvm.dbg.declare(metadata !{float* %vla}, metadata ![[VLA:.*]], metadata {{.*}})
; The VLA alloca and following store into the array should not be lowered to like this:
; CHECK-NOT:  call void @llvm.dbg.value(metadata !{float %r}, i64 0, metadata ![[VLA]])
; the backend interprets this as "vla has the location of %r".
  store float %r, float* %vla, align 4, !dbg !25, !tbaa !26
  tail call void @llvm.dbg.value(metadata !2, i64 0, metadata !18, metadata !{metadata !"0x102"}), !dbg !30
  %cmp8 = icmp sgt i32 %conv, 0, !dbg !30
  br i1 %cmp8, label %for.body, label %for.end, !dbg !30

for.body:                                         ; preds = %entry, %for.body.for.body_crit_edge
  %0 = phi float [ %.pre, %for.body.for.body_crit_edge ], [ %r, %entry ]
  %i.09 = phi i32 [ %inc, %for.body.for.body_crit_edge ], [ 0, %entry ]
  %arrayidx2 = getelementptr inbounds float* %vla, i32 %i.09, !dbg !31
  %div = fdiv float %0, %r, !dbg !31
  store float %div, float* %arrayidx2, align 4, !dbg !31, !tbaa !26
  %inc = add nsw i32 %i.09, 1, !dbg !30
  tail call void @llvm.dbg.value(metadata !{i32 %inc}, i64 0, metadata !18, metadata !{metadata !"0x102"}), !dbg !30
  %exitcond = icmp eq i32 %inc, %conv, !dbg !30
  br i1 %exitcond, label %for.end, label %for.body.for.body_crit_edge, !dbg !30

for.body.for.body_crit_edge:                      ; preds = %for.body
  %arrayidx2.phi.trans.insert = getelementptr inbounds float* %vla, i32 %inc
  %.pre = load float* %arrayidx2.phi.trans.insert, align 4, !dbg !31, !tbaa !26
  br label %for.body, !dbg !30

for.end:                                          ; preds = %for.body, %entry
  ret void, !dbg !32
}

; Function Attrs: nounwind readnone
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nounwind readnone
declare void @llvm.dbg.value(metadata, i64, metadata, metadata) #1

attributes #0 = { nounwind optsize readnone "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-realign-stack" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!20, !33}
!llvm.ident = !{!21}

!0 = metadata !{metadata !"0x11\0012\00clang version 3.4 \001\00\000\00\000", metadata !1, metadata !2, metadata !2, metadata !3, metadata !2, metadata !2} ; [ DW_TAG_compile_unit ] [/Volumes/Data/radar/15464571/<unknown>] [DW_LANG_C99]
!1 = metadata !{metadata !"<unknown>", metadata !"/Volumes/Data/radar/15464571"}
!2 = metadata !{i32 0}
!3 = metadata !{metadata !4}
!4 = metadata !{metadata !"0x2e\00run\00run\00\001\000\001\000\006\00256\001\002", metadata !5, metadata !6, metadata !7, null, void (float)* @run, null, null, metadata !10} ; [ DW_TAG_subprogram ] [line 1] [def] [scope 2] [run]
!5 = metadata !{metadata !"test.c", metadata !"/Volumes/Data/radar/15464571"}
!6 = metadata !{metadata !"0x29", metadata !5}          ; [ DW_TAG_file_type ] [/Volumes/Data/radar/15464571/test.c]
!7 = metadata !{metadata !"0x15\00\000\000\000\000\000\000", i32 0, null, null, metadata !8, null, null, null} ; [ DW_TAG_subroutine_type ] [line 0, size 0, align 0, offset 0] [from ]
!8 = metadata !{null, metadata !9}
!9 = metadata !{metadata !"0x24\00float\000\0032\0032\000\000\004", null, null} ; [ DW_TAG_base_type ] [float] [line 0, size 32, align 32, offset 0, enc DW_ATE_float]
!10 = metadata !{metadata !11, metadata !12, metadata !14, metadata !18}
!11 = metadata !{metadata !"0x101\00r\0016777217\000", metadata !4, metadata !6, metadata !9} ; [ DW_TAG_arg_variable ] [r] [line 1]
!12 = metadata !{metadata !"0x100\00count\003\000", metadata !4, metadata !6, metadata !13} ; [ DW_TAG_auto_variable ] [count] [line 3]
!13 = metadata !{metadata !"0x24\00int\000\0032\0032\000\000\005", null, null} ; [ DW_TAG_base_type ] [int] [line 0, size 32, align 32, offset 0, enc DW_ATE_signed]
!14 = metadata !{metadata !"0x100\00vla\004\008192", metadata !4, metadata !6, metadata !15} ; [ DW_TAG_auto_variable ] [vla] [line 4]
!15 = metadata !{metadata !"0x1\00\000\000\0032\000\000", null, null, metadata !9, metadata !16, i32 0, null, null, null} ; [ DW_TAG_array_type ] [line 0, size 0, align 32, offset 0] [from float]
!16 = metadata !{metadata !17}
!17 = metadata !{metadata !"0x21\000\00-1"}       ; [ DW_TAG_subrange_type ] [unbounded]
!18 = metadata !{metadata !"0x100\00i\006\000", metadata !19, metadata !6, metadata !13} ; [ DW_TAG_auto_variable ] [i] [line 6]
!19 = metadata !{metadata !"0xb\006\000\000", metadata !5, metadata !4} ; [ DW_TAG_lexical_block ] [/Volumes/Data/radar/15464571/test.c]
!20 = metadata !{i32 2, metadata !"Dwarf Version", i32 2}
!21 = metadata !{metadata !"clang version 3.4 "}
!22 = metadata !{i32 1, i32 0, metadata !4, null}
!23 = metadata !{i32 3, i32 0, metadata !4, null}
!24 = metadata !{i32 4, i32 0, metadata !4, null}
!25 = metadata !{i32 5, i32 0, metadata !4, null}
!26 = metadata !{metadata !27, metadata !27, i64 0}
!27 = metadata !{metadata !"float", metadata !28, i64 0}
!28 = metadata !{metadata !"omnipotent char", metadata !29, i64 0}
!29 = metadata !{metadata !"Simple C/C++ TBAA"}
!30 = metadata !{i32 6, i32 0, metadata !19, null}
!31 = metadata !{i32 7, i32 0, metadata !19, null}
!32 = metadata !{i32 8, i32 0, metadata !4, null}
!33 = metadata !{i32 1, metadata !"Debug Info Version", i32 2}
