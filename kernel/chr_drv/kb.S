/*
 * do_self 处理"普通"按键，即那些含义不会改变含义且只返回单个字符的按键
 */
do_self:
	lea alt_map,%ebx           ; 将alt_map（Alt键映射表）的地址加载到ebx
	testb $0x20,mode           ; 测试mode的第5位（0x20），判断是否按下了AltGr键
	jne 1f                     ; 若按下AltGr，跳转到标号1（使用alt_map）
	lea shift_map,%ebx         ; 否则加载shift_map（Shift键映射表）的地址到ebx
	testb $0x03,mode           ; 测试mode的低2位，判断是否按下了Shift键（左或右）
	jne 1f                     ; 若按下Shift，跳转到标号1（使用shift_map）
	lea key_map,%ebx           ; 否则加载默认映射表key_map的地址到ebx
1:	movb (%ebx,%eax),%al       ; 根据映射表和扫描码（eax）获取对应的ASCII码，存入al
	orb %al,%al                ; 检查al是否为0（0表示无对应字符）
	je none                    ; 若为0，跳转到none（不处理）
	testb $0x4c,mode           ; 测试mode的第2、6位（0x4c=0x40+0x0c），判断是否按下Ctrl或CapsLock
	je 2f                      ; 若未按下，跳转到标号2
	cmpb $'a,%al               ; 比较字符是否小于'a'（ASCII 97）
	jb 2f                      ; 若小于，跳转到标号2（不处理）
	cmpb $'},%al               ; 比较字符是否大于'}'（ASCII 125）
	ja 2f                      ; 若大于，跳转到标号2（不处理）
	subb $32,%al               ; 否则将字符转为大写（小写字母ASCII码减32）
2:	testb $0x0c,mode           ; 测试mode的第2、3位（0x0c），判断是否按下Ctrl键
	je 3f                      ; 若未按下，跳转到标号3
	cmpb $64,%al               ; 比较字符ASCII码是否小于64（'@'）
	jb 3f                      ; 若小于，跳转到标号3（不处理）
	cmpb $64+32,%al            ; 比较字符ASCII码是否大于等于96（'`'）
	jae 3f                     ; 若大于等于，跳转到标号3（不处理）
	subb $64,%al               ; 否则将字符转为Ctrl+字符的控制码（减64，如Ctrl+A=1）
3:	testb $0x10,mode           ; 测试mode的第4位（0x10），判断是否按下左Alt键
	je 4f                      ; 若未按下，跳转到标号4
	orb $0x80,%al              ; 否则将字符最高位设为1（表示Alt+字符）
4:	andl $0xff,%eax            ; 确保eax只保留低8位（字符值）
	xorl %ebx,%ebx             ; 将ebx清零（作为put_queue的参数）
	call put_queue             ; 调用put_queue，将字符放入终端输入队列
none:	ret                        ; 返回

/*
 * minus（减号）有单独的处理程序，因为减号扫描码前的'E0h'前缀
 * 表示按下的是小键盘的斜杠（/）
 */
minus:	cmpb $1,e0                ; 比较e0标志是否为1（e0=1表示有E0前缀）
	jne do_self                ; 若e0≠1，跳转到do_self（处理普通减号）
	movl $'/,%eax              ; 若e0=1，将eax设为'/'（小键盘斜杠）
	xorl %ebx,%ebx             ; ebx清零
	jmp put_queue              ; 跳转到put_queue，将'/'放入输入队列

/*
 * 该表决定获取扫描码后调用哪个处理程序。大多数程序
 * 根据是按下（make）还是释放（break），调用do_self或none
 */
key_table:
	.long none,do_self,do_self,do_self	/* 00-03：无  esc 1 2 */
	.long do_self,do_self,do_self,do_self	/* 04-07：3 4 5 6 */
	.long do_self,do_self,do_self,do_self	/* 08-0B：7 8 9 0 */
	.long do_self,do_self,do_self,do_self	/* 0C-0F：+ ' Backspace Tab */
	.long do_self,do_self,do_self,do_self	/* 10-13：q w e r */
	.long do_self,do_self,do_self,do_self	/* 14-17：t y u i */
	.long do_self,do_self,do_self,do_self	/* 18-1B：o p } ^ */
	.long do_self,ctrl,do_self,do_self	/* 1C-1F：Enter Ctrl a s */
	.long do_self,do_self,do_self,do_self	/* 20-23：d f g h */
	.long do_self,do_self,do_self,do_self	/* 24-27：j k l | */
	.long do_self,do_self,lshift,do_self	/* 28-2B：{ 左Shift , */
	.long do_self,do_self,do_self,do_self	/* 2C-2F：z x c v */
	.long do_self,do_self,do_self,do_self	/* 30-33：b n m . */
	.long do_self,minus,rshift,do_self	/* 34-37：, - 右Shift * */
	.long alt,do_self,caps,func		/* 38-3B：Alt 空格 CapsLock F1 */
	.long func,func,func,func		/* 3C-3F：F2 F3 F4 F5 */
	.long func,func,func,func		/* 40-43：F6 F7 F8 F9 */
	.long func,num,scroll,cursor		/* 44-47：F10 NumLock Scroll Lock Home */
	.long cursor,cursor,do_self,cursor	/* 48-4B：上箭头 PageUp - 左箭头 */
	.long cursor,cursor,do_self,cursor	/* 4C-4F：小键盘5 右箭头 + End */
	.long cursor,cursor,cursor,cursor	/* 50-53：下箭头 PageDown Insert Delete */
	.long none,none,do_self,func		/* 54-57：SysRq ? < F11 */
	.long func,none,none,none		/* 58-5B：F12 ? ? ? */
	.long none,none,none,none		/* 5C-5F：? ? ? ? */
	/* 以下为未使用或预留的扫描码（60-FF），均指向none（不处理） */
	.long none,none,none,none		/* 60-63 ? ? ? ? */
	.long none,none,none,none		/* 64-67 ? ? ? ? */
	.long none,none,none,none		/* 68-6B ? ? ? ? */
	.long none,none,none,none		/* 6C-6F ? ? ? ? */
	.long none,none,none,none		/* 70-73 ? ? ? ? */
	.long none,none,none,none		/* 74-77 ? ? ? ? */
	.long none,none,none,none		/* 78-7B ? ? ? ? */
	.long none,none,none,none		/* 7C-7F ? ? ? ? */
	.long none,none,none,none		/* 80-83 ? 释放 释放 释放 */
	.long none,none,none,none		/* 84-87 释放 释放 释放 释放 */
	.long none,none,none,none		/* 88-8B 释放 释放 释放 释放 */
	.long none,none,none,none		/* 8C-8F 释放 释放 释放 释放 */
	.long none,none,none,none		/* 90-93 释放 释放 释放 释放 */
	.long none,none,none,none		/* 94-97 释放 释放 释放 释放 */
	.long none,none,none,none		/* 98-9B 释放 释放 释放 释放 */
	.long none,unctrl,none,none		/* 9C-9F 释放 释放Ctrl 释放 释放 */
	.long none,none,none,none		/* A0-A3 释放 释放 释放 释放 */
	.long none,none,none,none		/* A4-A7 释放 释放 释放 释放 */
	.long none,none,unlshift,none		/* A8-AB 释放 释放 释放左Shift 释放 */
	.long none,none,none,none		/* AC-AF 释放 释放 释放 释放 */
	.long none,none,none,none		/* B0-B3 释放 释放 释放 释放 */
	.long none,none,unrshift,none		/* B4-B7 释放 释放 释放右Shift 释放 */
	.long unalt,none,uncaps,none		/* B8-BB 释放Alt 释放 释放CapsLock 释放 */
	.long none,none,none,none		/* BC-BF 释放 释放 释放 释放 */
	.long none,none,none,none		/* C0-C3 释放 释放 释放 释放 */
	.long none,none,none,none		/* C4-C7 释放 释放 释放 释放 */
	.long none,none,none,none		/* C8-CB 释放 释放 释放 释放 */
	.long none,none,none,none		/* CC-CF 释放 释放 释放 释放 */
	.long none,none,none,none		/* D0-D3 释放 释放 释放 释放 */
	.long none,none,none,none		/* D4-D7 释放 释放 释放 释放 */
	.long none,none,none,none		/* D8-DB 释放 ? ? ? */
	.long none,none,none,none		/* DC-DF ? ? ? ? */
	.long none,none,none,none		/* E0-E3 e0前缀 e1前缀 ? ? */
	.long none,none,none,none		/* E4-E7 ? ? ? ? */
	.long none,none,none,none		/* E8-EB ? ? ? ? */
	.long none,none,none,none		/* EC-EF ? ? ? ? */
	.long none,none,none,none		/* F0-F3 ? ? ? ? */
	.long none,none,none,none		/* F4-F7 ? ? ? ? */
	.long none,none,none,none		/* F8-FB ? ? ? ? */
	.long none,none,none,none		/* FC-FF ? ? ? ? */

/*
 * kb_wait 等待键盘控制器缓冲区为空
 * 没有超时机制 - 若缓冲区一直不为空，系统会挂起
 */
kb_wait:
	pushl %eax         ; 保存eax寄存器
1:	inb $0x64,%al      ; 从键盘控制器端口（0x64）读取状态字节到al
	testb $0x02,%al    ; 测试状态字节的第1位（0x02），判断输出缓冲区是否满
	jne 1b             ; 若缓冲区满（非0），跳转到标号1继续等待
	popl %eax          ; 恢复eax寄存器
	ret                ; 返回

/*
 * 该程序通过让键盘控制器拉低复位线来重启机器
 */
reboot:
	call kb_wait       ; 调用kb_wait，等待键盘控制器就绪
	movw $0x1234,0x472 ; 向0x472地址写入0x1234（告诉BIOS不进行内存检查）
	movb $0xfc,%al     ; al = 0xfc（键盘控制器命令：脉冲复位线和A20线为低）
	outb %al,$0x64     ; 将命令发送到键盘控制器端口（0x64）
die:	jmp die            ; 无限循环（等待重启）
