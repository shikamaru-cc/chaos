; C prototype
; void switch_to(struct task_struct* cur, struct task_struct* next);

[bits 32]
section .text
global switch_to
switch_to:
  push esi
  push edi
  push ebx
  push ebp

  ; save current thread esp in task_struct.self_kstack
  mov eax, [esp+20] ; arg cur
  mov [eax], esp
  ; load next thread task_struct.self_kstack to esp
  mov eax, [esp+24] ; arg next
  mov esp, [eax]

  pop ebp
  pop ebx
  pop edi
  pop esi
  ret
