UNIT MGE;
{ Last UpGrade 19.8.1998 , The VERY first version came out of daylight 1993 }
{ Marko's_Graphics_Engine;   by Marko Kallinki 1996,98 (C) Vaasa ; Finland  }
{ You are free to use these pieces of code if you give me some gredits  ð:) }
{ I have to give some gredits to SWAG-Team and Asphyxia-Team too... }
{ Have a fun , it's all yours... }

INTERFACE
Uses    Crt,MCE;
Type    Screens  = ^Screen;
        Screen   = array[0..63999] of char;
        Palettes = ^Palette;
        Palette  = array[1..768] of char;
Var
        x,y,z,t,tt,t1,t2:integer; vir,vir1,c1,c2,fill:byte;

        VGA1,VGA2,VGA3,VGA4,VGA5:Screens;
        PAL1                    :Palettes;
        S1,S2,S3,S4,S5,S6,S7    :word;
        w1,w2,w3,w4,w5          :word;
        Source,Dest             :word;
        Seg1,Off1,Off2          :word;
        GFile                   :File;

                          { speed index = x screens per second }
                  { the index have been calculated with my 75Mhz Pentium }
Procedure SetMCGA;
Procedure SetText;
Function  VGA(p3:byte):word;
Procedure SetVirtual(vir,vir1:byte);
Procedure SetVirtualOff;
Procedure SetFill(p1:byte);
Procedure PSet(p1,p2:byte);
Procedure ClrScreen(p1,c1:byte);
Procedure O_PPix(off1:word;c1:byte);                           {i22}
Function  O_GPix(off1:word):byte;                              {i22}
Procedure PPix(x1,y1:integer;c1:byte);                         {i20}
Function  GPix(x1,y1:integer):byte;                            {i20}
Function  GSeg(p1:byte):word;
Procedure {-not-}FMove(var xsource, xdest; size: word);
Procedure CopyScr(p1,p2:byte);                                 {i272}
Procedure PMove(x1,y1,x2,y2,x3,y3:integer);                    {i162}
Procedure SMove(x1,y1,x2,y2,x3,y3:integer);                    {i118}
Procedure XSMove(x1,y1,x2,y2,x3,y3:integer);                   {i91}
Procedure YSMove(x1,y1,x2,y2,x3,y3:integer);                   {i91}
Procedure ZSMove(x1,y1,x2,y2,x3,y3:integer);                   {i76}
Procedure SaveMCE(s1:string;c1:byte);
Procedure LoadMCE(s1:string);
Procedure SaveCEL(s1:string);
Procedure LoadCEL(s1:string);
Procedure HLine(x1,y1,x2:integer;c1:byte);                     {i118}
Procedure VLine(x1,y1,y2:integer;c1:byte);                     {i96}
Procedure HCLine(x1,y1,x2,c1,c2:integer);                      {i112}
Procedure SLine(x1,y1,x2:integer;c1:byte);                     {i ? }
Procedure Explo(x1,y1,x2,y2,x3,y3:integer); { GreatPal }       {i ? }
Procedure Visib(x1,y1,x2,y2,x3,y3:integer;c1:byte);{ GreatPal }{i ? }
Procedure {-not-}FillLine(x1,y1,x2,c1,c2:integer);             {i ? }
Procedure Box(x1,y1,x2,y2:integer;c1,c2:byte);                 {i113}
Procedure FBox(x1,y1,x2,y2:integer;c1:byte);                   {i118}
Procedure Rectangle(x1,y1,x2,y2:integer;c1:byte);              {i108}
Procedure Line(x1,y1,x2,y2:integer;c1:byte);                   {i90}
Procedure Ellipse(x1,y1, ea,eb :Integer; c1:byte);             {i22}
Procedure Disc(x1,y1, ea,eb :Integer; c1,c2:byte);             {i83}
Procedure Polygon(x1,y1,x2,y2,x3,y3,x4,y4:integer;c1,c2:byte); {i107}
Procedure Zoom(x1,y1,x2,y2,x3,y3,x4,y4:integer);               {i56}
Procedure WriteXY(x1,y1:integer;s:string;c1:byte;c2:shortint); {i43}
Procedure PPal(p1,c1,rr,gg,bb:byte);
Procedure GPal(p1,c1:byte;var r,g,b:byte);
Procedure ShowPal(x1,x2:integer);
Procedure ScrollPal(x1,x2,x3:integer);
Procedure FadeTo(x1,x2,x3:integer);
Procedure FadeUp(x1,x2:integer);

IMPLEMENTATION

Procedure SetMCGA;Assembler; Asm mov ax,0013h; int 10h End;
Procedure SetText;Assembler; Asm mov ax,0003h; int 10h End;
{-----------------------------------------------------------}
Function VGA(p3:byte):word; Begin case p3 of
0: VGA:= $a000; 1: VGA:= S1; 2: VGA:= S2;
3: VGA:=    S3; 4: VGA:= S4; 5: VGA:= S5; end End;
{-----------------------------------------------------------}
Procedure SetVirtual(vir,vir1:byte); Begin
  if vir>0 then  begin; GetMem (VGA1,64000);  S1 := seg (VGA1^) end;
  if vir>1 then  begin; GetMem (VGA2,64000);  S2 := seg (VGA2^) end;
  if vir>2 then  begin; GetMem (VGA3,64000);  S3 := seg (VGA3^) end;
  if vir>3 then  begin; GetMem (VGA4,64000);  S4 := seg (VGA4^) end;
  if vir>4 then  begin; GetMem (VGA5,64000);  S5 := seg (VGA5^) end;
  if vir1>0 then begin; GetMem (PAL1,768);    S6 := seg (PAL1^) end;
  Source:=$a000; Dest:=$a000; End;
{-----------------------------------------------------------}
Procedure SetVirtualOff; Begin if vir>0 then FreeMem (VGA1,64000);
  if vir>1 then FreeMem (VGA2,64000); if vir>2 then FreeMem (VGA3,64000);
  if vir>3 then FreeMem (VGA4,64000); if vir>4 then FreeMem (VGA5,64000);
  if vir1>0 then FreeMem (PAL1,768); End;
{-----------------------------------------------------------}
Procedure SetFill(p1:byte); Begin; Fill:=p1 mod 3 End;
{-----------------------------------------------------------}
Procedure PSet(p1,p2:byte); Begin; Source:=VGA(p1); Dest:=VGA(p2) End;
{-----------------------------------------------------------}
Procedure ClrScreen(p1,c1:byte);
Begin FillChar(Mem[VGA(p1):0],64000,c1) End;
{-----------------------------------------------------------}
Procedure O_PPix(off1:word;c1:byte);Assembler;
Asm mov es,[dest]; mov di,[off1]; mov al,c1; mov es:[di],al End;
{-----------------------------------------------------------}
Function  O_GPix(off1:word):byte;Assembler;
Asm mov es,[Source]; mov di,[off1]; mov al,es:[di] End;
{-----------------------------------------------------------}
Procedure PPix(x1,y1:integer;c1:byte);Assembler;
Asm mov es,[dest]; mov ax,y1; shl ax,6; mov di,ax; shl ax,2;
  add di,ax; add di,x1; mov al,c1; mov es:[di],al End;
{-----------------------------------------------------------}
Function GPix(x1,y1:integer):byte;Assembler;
Asm mov es,[source]; mov ax,y1; shl ax,6; mov di,ax;
  shl ax,2; add di,ax; add di,x1; mov al,es:[di] End;
{-----------------------------------------------------------}
Function GSeg(p1:byte):word; Begin GSeg:=VGA(p1); End;
{-----------------------------------------------------------}
Procedure FMove(var xsource, xdest; size: word);Assembler;
Asm push ds; push es; lds si,xsource; les di,xdest
  mov cx,size; cld; shr cx,1; jnc @word1; movsb
@word1:
{$IFDEF USE386}
  shr cx,1; jnc @word2; movsw
@word2: db 0f3h, 066h, 0a5h
{$ELSE}
  rep movsw
{$ENDIF}
  pop es; pop ds End;
{-----------------------------------------------------------}
Procedure CopyScr(p1,p2:byte);
Begin FMove(Mem[VGA(p1):0],Mem[VGA(p2):0],64000) End;
{-----------------------------------------------------------}
Procedure PMove(x1,y1,x2,y2,x3,y3:integer); Begin
asm mov cx,x1; mov bx,x2; sub bx,cx; mov t,bx
    mov ax,y1; shl ax,6; mov t1,ax; shl ax,2; add ax,cx; add t1,ax
    mov ax,y3; shl ax,6; mov t2,ax; shl ax,2; add ax,x3; add t2,ax end;
for tt:=y1 to y2 do begin FMove(Mem[Source:t1],Mem[Dest:t2],t);
    asm add t1,101000000b; add t2,101000000b end end End;
{-----------------------------------------------------------}
{Procedure SMove(x1,y1,x2,y2,x3,y3:integer);Assembler;  [ 8 bit ]
Asm inc x2; inc y2; mov cx,x1; mov dx,y1; mov ax,dx; shl ax,6; mov bx,ax; shl ax,2
  add ax,bx; add ax,cx; mov di,ax; mov ax,y3; shl ax,6; mov bx,ax; shl ax,2
  add ax,bx; add ax,x3; mov bx,ax; mov ax,320; sub ax,x2; add ax,cx; mov z,ax
  mov si,y2; sub si,dx; mov ax,x2; sub ax,cx; mov x,ax; mov es,[source]
@LoopY: mov cx,x; @LoopX: mov al,es:[di]; cmp al,0; je @Over
  mov es,[dest]; mov dx,di; mov di,bx; mov es:[di],al; mov es,[source]; mov di,dx
@Over: inc di; inc bx; dec cx; jnz @LoopX; mov ax,z; add di,ax; add bx,ax; dec si; cmp si,0; jg @LoopY End;  }
{-----------------------------------------------------------}
Procedure SMove(x1,y1,x2,y2,x3,y3:integer);Assembler;  { 16 bit }
Asm mov ax,x2; sub ax,x1; shr ax,1; jnc @Start; inc x2;
    @Start: inc y2; mov cx,x1; mov dx,y1; mov ax,dx
    shl ax,6; mov bx,ax; shl ax,2; add ax,bx; add ax,cx
    mov di,ax; mov ax,y3; shl ax,6; mov bx,ax; shl ax,2
    add ax,bx; add ax,x3; mov bx,ax; mov ax,320; sub ax,x2
    add ax,cx; mov z,ax; mov si,y2; sub si,dx; mov ax,x2
    sub ax,cx; shr ax,1; mov x,ax; mov es,[source]

@LoopY: mov cx,x
@LoopX: mov ax,es:[di]
        cmp ah,0; jne @Lo&Hi; cmp al,0; jne @LowB; jmp @ZeroB
@Lo&Hi: cmp al,0; je @HighB; mov es,[dest]; mov dx,di; mov di,bx
        mov es:[di],ax; mov es,[source]; mov di,dx; jmp @ZeroB
@HighB: mov es,[dest]; mov dx,di; mov di,bx; inc di
        mov es:[di],ah; mov es,[source]; mov di,dx; jmp @ZeroB
@LowB:  mov es,[dest]; mov dx,di; mov di,bx
        mov es:[di],al; mov es,[source]; mov di,dx
@ZeroB: add di,2; add bx,2; dec cx; jnz @LoopX
        mov ax,z; add di,ax; add bx,ax
        dec si; cmp si,0; jg @LoopY End;
{-----------------------------------------------------------}
{Procedure SMove32(x1,y1,x2,y2,x3,y3:integer);Assembler;
 mov dx,x2; inc dx; @LOOPY:; @LOOPX:; @EMPTY: lodsb; inc di; cmp di,dx; jg FULL0; cmp al,0; jne @FULL; jmp @EMPTY
 @FULL0: dec di; mov bx,di; @FULL:  lodsb; inc di; cmp di,dx; jg LOOPY; cmp al,0; je @MOVE; jmp @FULL2
 @MOVE:  dec di; sub bx,di; mov cx,bx; shr cx,2; db 0f3h,066h,0a5h; shl cx,2; sub bx,cx;
 shr cx,1; rep movsw; shl cx,1; sub bx,cx; dec cx; jnz @EMPTY; movsb; jmp @EMPTY;
 jne @LOOPX:; jne @LOOPY:;}
{-----------------------------------------------------------}
Procedure XSMove(x1,y1,x2,y2,x3,y3:integer);Assembler;
Asm inc x2; inc y2; mov ax,y1; shl ax,6; mov bx,ax
  shl ax,2; add ax,bx; add ax,x1; dec ax; mov dx,ax
  mov ax,x2; sub ax,x1; mov x,ax
  mov ax,y3; shl ax,6; mov bx,ax; shl ax,2
  add ax,bx; add ax,x3; add ax,x; dec ax; mov bx,ax
  mov ax,320; sub ax,x2; add ax,x1; mov z,ax
  mov si,y2; sub si,y1; mov ax,x2; sub ax,x1
  mov x,ax; mov es,[source]
@LoopY: mov cx,x
@LoopX: mov di,dx; mov al,es:[di]
  cmp al,0; je @Over
  mov es,[dest]; mov di,bx
  mov es:[di],al; mov es,[source]
@Over: inc dx; dec bx; dec cx; jnz @LoopX
  mov ax,z; add dx,ax; mov ax,bx; add ax,z
  add ax,x; add ax,x; mov bx,ax
  dec si; cmp si,0; jg @LoopY End;
{-----------------------------------------------------------}
Procedure YSMove(x1,y1,x2,y2,x3,y3:integer);Assembler;
Asm inc x2; inc y2; mov ax,y1; shl ax,6; mov bx,ax
  shl ax,2; add ax,bx; add ax,x1; mov dx,ax
  mov si,y2; sub si,y1;mov ax,y3;
  add ax,si; dec ax; shl ax,6; mov bx,ax
  shl ax,2; add ax,bx; add ax,x3; mov bx,ax
  mov ax,320; sub ax,x2; add ax,x1; mov z,ax
  mov ax,x2; sub ax,x1; mov x,ax; mov es,[source]
@LoopY: mov cx,x
@LoopX: mov di,dx; mov al,es:[di]
  cmp al,0; je @Over
  mov es,[dest]; mov di,bx
  mov es:[di],al; mov es,[source]
@Over: inc dx; inc bx; dec cx; jnz @LoopX
  mov ax,z; add dx,ax; add bx,ax; sub bx,640
  dec si; cmp si,0; jg @LoopY End;
{-----------------------------------------------------------}
Procedure ZSMove(x1,y1,x2,y2,x3,y3:integer);Assembler;
Asm inc x2; inc y2; mov ax,y1; shl ax,6; mov bx,ax
  shl ax,2; add ax,bx; add ax,x1; mov dx,ax
  mov ax,x2; sub ax,x1; mov x,ax; mov si,y2; sub si,y1;
  mov ax,y3; add ax,si; dec ax; shl ax,6; mov bx,ax; shl ax,2
  add ax,bx; add ax,x3; add ax,x; dec ax; mov off2,ax
  mov ax,320; sub ax,x2; add ax,x1; mov z,ax
  mov bx,[source]; mov es,bx
@LoopY: mov cx,x
@LoopX: mov di,dx; mov al,es:[di]
  cmp al,0; je @Over
  mov es,[dest]; mov di,[off2]
  mov es:[di],al; mov es,[source]
@Over: inc dx; dec off2; dec cx; jnz @LoopX
  mov ax,z; add dx,ax; mov ax,off2; add ax,z
  add ax,x; add ax,x; sub ax,640; mov off2,ax
  dec si; cmp si,0; jg @LoopY End;
{-----------------------------------------------------------}
Procedure SaveMCE(s1:string;c1:byte);Begin
  Assign(GFile,s1); Rewrite(GFile,1);
  W1:=19702; BlockWrite(GFile,W1,2);
  W1:=17644; BlockWrite(GFile,W1,2);
  W1:=c1;    BlockWrite(GFile,W1,2);
  BlockWrite(GFile,PAL1^,768);
if c1 and 2<>0 then begin Compress(GSeg(2),GSeg(1),64000);
  BlockWrite(GFile,VGA1^,CompressLength); end;
if c1 and 4<>0 then begin Compress(GSeg(3),GSeg(1),64000);
  BlockWrite(GFile,VGA1^,CompressLength); end;
if c1 and 8<>0 then begin Compress(GSeg(4),GSeg(1),64000);
  BlockWrite(GFile,VGA1^,CompressLength); end;
if c1 and 16<>0 then begin Compress(GSeg(5),GSeg(1),64000);
  BlockWrite(GFile,VGA1^,CompressLength); end;
Close(GFile); End;
{-----------------------------------------------------------}
Procedure LoadMCE(s1:string);Begin
  Assign(GFile,s1); Reset(GFile,1);
  BlockRead(GFile,W1,2);
  BlockRead(GFile,W1,2);
  BlockRead(GFile,W1,2); c1:=W1;
  BlockRead(GFile,PAL1^,768);
if c1 and 2<>0 then begin BlockRead(GFile,W1,2);
  BlockRead(GFile,VGA1^,W1-2); UnCompress(GSeg(1),GSeg(2),64000); end;
if c1 and 4<>0 then begin BlockRead(GFile,W1,2);
  BlockRead(GFile,VGA1^,W1-2); UnCompress(GSeg(1),GSeg(3),64000); end;
if c1 and 8<>0 then begin BlockRead(GFile,W1,2);
  BlockRead(GFile,VGA1^,W1-2); UnCompress(GSeg(1),GSeg(4),64000); end;
if c1 and 16<>0 then begin BlockRead(GFile,W1,2);
  BlockRead(GFile,VGA1^,W1-2); UnCompress(GSeg(1),GSeg(5),64000); end;
  Close(GFile); End;
{-----------------------------------------------------------}
Procedure SaveCEL(s1:string); Begin
  Assign(GFile,s1); Rewrite(GFile,1);
  BlockWrite(GFile,PAL1^,768); BlockWrite(GFile,VGA1^,64000);
  Close(GFile); End;
{-----------------------------------------------------------}
Procedure LoadCEL(s1:string);Begin
  Assign(GFile,s1); Reset(GFile,1);
  BlockRead(Gfile,PAL1^,768); BlockRead(GFile,VGA1^,64000);
  Close(GFile); End;
{-----------------------------------------------------------}
Procedure HLine(x1,y1,x2:integer;c1:byte);Assembler;
Asm mov es,[dest]; mov ax,y1; shl ax,6; mov di,ax; shl ax,2;
    add di,ax;add di,x1; mov cx,x2; sub cx,x1; mov al,c1; db $f3,$66,$AA End;
{-----------------------------------------------------------}
Procedure VLine(x1,y1,y2:integer;c1:byte);Assembler;
Asm mov es,[dest]; mov ax,y1; shl ax,6; mov di,ax; shl ax,2;
  add di,ax; add di,x1; mov al,c1; mov cx,y2; sub cx,y1; @J1:
  mov es:[di],al; add di,140h; dec cx; jnz @J1 End;
{-----------------------------------------------------------}
Procedure HCLine(x1,y1,x2,c1,c2:integer); { My Greatest Code Ever ! }
Begin asm mov ax,x2; cmp ax,x1; jne @j1; add ax,1; mov x2,ax;
          @j1:; inc c2; mov ax,y1; shl ax,6; mov bx,ax; shl ax,2
          add ax,bx; add ax,x1; mov off1,ax end;
if c1<c2 then begin asm xor dx,dx; xor si,si; mov ax,c2; sub ax,c1;
      shl ax,10; mov cx,x2; sub cx,x1; div cx; mov dx,ax
      mov es,[dest]; mov di,off1; @L1:; mov ax,dx;
      add si,ax; mov ax,si; shr ax,10; add ax,c1; inc di
      mov es:[di],al; dec cx; jnz @L1 end; PPix(x2,y1,c2-1);
end else begin asm xor dx,dx; xor si,si; mov ax,c1; sub ax,c2; shl ax,10
      mov cx,x2; sub cx,x1; div cx; mov dx,ax; mov es,[dest]
      mov di,off1; @L1:; mov ax,dx; add si,ax; mov ax,si
      shr ax,10; mov bx,c1; sub bx,ax; mov ax,bx; inc di
      mov es:[di],al; dec cx; jnz @L1 end; PPix(x2,y1,c2+1) end End;
{-----------------------------------------------------------}
Procedure SLine(x1,y1,x2:integer;c1:byte);Assembler;
Asm mov di,y1; shl di,6; mov bx,di; shl di,2; add di,bx; add di,x1;
    mov es,Source; mov cx,x2; sub cx,x1; shr cx,1;
    jnc @S; mov al,es:[di]; add al,c1; stosb; @S:
    mov ax,es:[di]; and al,15; add al,c1;
    and ah,15; add ah,c1; stosw; dec cx; jnz @S End;
{-----------------------------------------------------------}
Procedure Explo(x1,y1,x2,y2,x3,y3:integer);Assembler; { requires GreatPal }
Asm inc x2; inc y2; mov cx,x1; mov dx,y1; mov ax,dx; shl ax,6; mov bx,ax; shl ax,2
  add ax,bx; add ax,cx; mov di,ax; mov ax,y3; shl ax,6; mov bx,ax; shl ax,2
  add ax,bx; add ax,x3; mov bx,ax; mov ax,320; sub ax,x2; add ax,cx; mov z,ax
  mov si,y2; sub si,dx; mov ax,x2; sub ax,cx; mov x,ax; mov es,[source]
@LoopY: mov cx,x; @LoopX: mov al,es:[di]; cmp al,0; je @Over
  mov es,[dest]; mov dx,di; mov di,bx; mov c1,al; mov al,es:[di]; and al,15;
  shr al,1; add al,c1; mov es:[di],al; mov es,[source]; mov di,dx
@Over: inc di; inc bx; dec cx; jnz @LoopX;
  mov ax,z; add di,ax; add bx,ax; dec si; cmp si,0; jg @LoopY End;
{-----------------------------------------------------------}
Procedure Visib(x1,y1,x2,y2,x3,y3:integer;c1:byte);Assembler; { requires GreatPal }
Asm inc x2; inc y2; mov cx,x1; mov dx,y1; mov ax,dx; shl ax,6; mov bx,ax; shl ax,2
  add ax,bx; add ax,cx; mov di,ax; mov ax,y3; shl ax,6; mov bx,ax; shl ax,2
  add ax,bx; add ax,x3; mov bx,ax; mov ax,320; sub ax,x2; add ax,cx; mov z,ax
  mov si,y2; sub si,dx; mov ax,x2; sub ax,cx; mov x,ax; mov es,[source]
@LoopY: mov cx,x; @LoopX: mov al,es:[di]; cmp al,0; je @Over
  mov es,[dest]; mov dx,di; mov di,bx; mov al,es:[di]; and al,15;
  shr al,1; add al,c1; mov es:[di],al; mov es,[source]; mov di,dx
@Over: inc di; inc bx; dec cx; jnz @LoopX;
  mov ax,z; add di,ax; add bx,ax; dec si; cmp si,0; jg @LoopY End;
{-----------------------------------------------------------}
Procedure FillLine(x1,y1,x2,c1,c2:integer); Begin
  if fill=0 then HLine(x1,y1,x2,c1);
  if fill=1 then HCLine(x1,y1,x2,c1,c2);
  if fill=2 then SLine(x1,y1,x2,c1); End;
{-----------------------------------------------------------}
Procedure Box(x1,y1,x2,y2:integer;c1,c2:byte);
Begin For t1:=y1 to y2 do FillLine(x1,t1,x2,c1,c2);End;
{-----------------------------------------------------------}
Procedure FBox(x1,y1,x2,y2:integer;c1:byte);Assembler;
Asm mov ax,y1; shl ax,6; mov di,ax; shl ax,2; add di,ax;
    add di,x1; mov bx,y2; sub bx,y1; mov al,c1; mov dx,140h;
    sub dx,x2; add dx,x1; mov es,[dest]
    @Loop: mov cx,x2; sub cx,x1; db $f3,$66,$AA
    add di,dx; dec bx; cmp bx,0; jne @Loop End;
{-----------------------------------------------------------}
Procedure Rectangle(x1,y1,x2,y2:integer;c1:byte);
Begin HLine(x1,y1,x2,c1);Hline(x1,y2,x2,c1);
      Vline(x1,y1,y2,c1);Vline(x2,y1,y2+1,c1);End;
{-----------------------------------------------------------}
Procedure Line(x1,y1,x2,y2:integer;c1:byte);
  Function sgn(a:real):integer;
  Begin if a>0 then sgn:=+1;
        if a<0 then sgn:=-1;
        if a=0 then sgn:=0; End;
Var d1x,d1y,d2x,d2y,u,v,m,n:integer;
Begin  u:= x2 - x1; v:= y2 - y1; d1x:= SGN(u); d1y:= SGN(v);
       d2x:= SGN(u); d2y:= 0; m:= ABS(u); n:= ABS(v);
     if not (M>N) then begin d2x:= 0 ; d2y:= SGN(v);
       m:= ABS(v); n:= ABS(u); end;
       asm mov bx,m; mov dx,bx; shr dx,1; mov si,n;
       mov ax,y1; shl ax,6; mov cx,ax; shl ax,2
       add ax,cx; add ax,x1; mov di,ax
       mov ax,d1y; shl ax,6; mov cx,ax; shl ax,2
       add ax,cx; add ax,d1x; mov d1y,ax
       mov ax,d2y; shl ax,6; mov cx,ax; shl ax,2
       add ax,cx; add ax,d2x; mov d2y,ax
       mov es,[dest]; mov al,c1; mov cx,bx
       @L1: mov es:[di],al; add dx,si; cmp dx,bx; jle @J1
       sub dx,bx; add di,d1y; jmp @E1; @J1: add di,d2y
       @E1: dec cx; jnz @L1 end End;
{-----------------------------------------------------------}
Procedure Ellipse(x1,y1, ea,eb :Integer; c1:byte);
Var elx, ely : Integer; aa, aa2, bb, bb2, d, dx, dy : LongInt;
Begin
  elx := 0; ely := eb; aa := LongInt(ea) * ea; aa2 := aa shl 1;
  bb := LongInt(eb) * eb; bb2 := bb shl 1;
  d := bb - aa * eb + aa shr 2; dx := 0; dy := aa2 * eb;
  PPix(x1, y1 - ely, c1); PPix(x1, y1 + ely, c1);
  PPix(x1 - ea, y1, c1); PPix(x1 + ea, y1, c1);

  WHILE (dx < dy) DO BEGIN
    IF (d > 0) THEN BEGIN Dec(ely); Dec(dy, aa2); Dec(d, dy); END;
    inc(elx); inc(dx, bb2); inc(d, bb + dx);

    asm mov ax,y1; add ax,ely; shl ax,6; mov t1,ax;
        shl ax,2; add ax,x1; add t1,ax end;
    O_PPix( t1 + elx , c1);O_PPix( t1 - elx , c1);
    asm mov ax,y1; sub ax,ely; shl ax,6; mov t1,ax;
        shl ax,2; add ax,x1; add t1,ax end;
    O_PPix( t1 + elx , c1); O_PPix( t1 - elx , c1) end;

  Inc(d, (3 * (aa - bb) div 2 - (dx + dy)) div 2);
  WHILE (ely > 0) DO BEGIN
    IF (d < 0) THEN BEGIN Inc(elx); Inc(dx, bb2); Inc(d, bb + dx); END;
    Dec(ely); Dec(dy, aa2); Inc(d, aa - dy);

    asm mov ax,y1; add ax,ely; shl ax,6; mov t1,ax;
        shl ax,2; add ax,x1; add t1,ax end;
    O_PPix( t1 + elx, c1); O_PPix( t1 - elx, c1);
    asm mov ax,y1; sub ax,ely; shl ax,6; mov t1,ax;
        shl ax,2; add ax,x1; add t1,ax end;
    O_PPix( t1 + elx, c1); O_PPix( t1 - elx, c1) end End;
{-----------------------------------------------------------}
Procedure Disc(x1,y1, ea,eb :Integer; c1,c2:byte);
Var elx, ely : Integer; aa, aa2, bb, bb2, d, dx, dy : LongInt;
Begin
  elx := 0; ely := eb; aa := LongInt(ea) * ea; aa2 := aa shl 1;
  bb := LongInt(eb) * eb; bb2 := bb shl 1;
  d := bb - aa * eb + aa shr 2; dx := 0; dy := aa2 * eb;
  while (dx < dy) do begin
    if (d > 0) then begin Dec(ely); Dec(dy, aa2); Dec(d, dy); end;
    inc(elx); inc(dx, bb2); inc(d, bb + dx);
    if not (ely = w1) then begin
      FillLine((x1-elx),(y1+ely),(x1+elx),c1,c2);
      FillLine((x1-elx),(y1-ely),(x1+elx),c1,c2); end;
    w1:=ely; end;
      SLine( (x1-ea),(y1),(x1+ea),c1);

  Inc(d, (3 * (aa - bb) div 2 - (dx + dy)) div 2);
  while (ely > 1) do begin
    if (d < 0) then begin Inc(elx); Inc(dx, bb2); Inc(d, bb + dx); end;
    Dec(ely); Dec(dy, aa2); Inc(d, aa - dy);
    if not (ely = w1) then begin
    FillLine((x1-elx),(y1-ely),(x1+elx),c1,c2);
    FillLine((x1-elx),(y1+ely),(x1+elx),c1,c2); end;
    w1:=ely; end; End;
{-----------------------------------------------------------}
Procedure Polygon(x1,y1,x2,y2,x3,y3,x4,y4:integer;c1,c2:byte);
var vx,mny,mxy:integer;
    mnx,mxx,yc:integer;
    mul1,mul2,mul3,mul4:integer;
    div1,div2,div3,div4:integer;
Begin asm
  mov ax,y1; mov bx,y1; mov cx,y2
  cmp cx,ax; jae @J1; mov ax,cx; @J1:
  cmp cx,bx; jle @J2; mov bx,cx; @J2:; mov cx,y3;
  cmp cx,ax; jae @J3; mov ax,cx; @J3:
  cmp cx,bx; jle @J4; mov bx,cx; @J4:; mov cx,y4;
  cmp cx,ax; jae @J5; mov ax,cx; @J5:
  cmp cx,bx; jle @J6; mov bx,cx; @J6:
  mov mny,ax; mov mxy,bx end;

  mul1:=x1-x4; div1:=y1-y4; mul2:=x2-x1; div2:=y2-y1;
  mul3:=x3-x2; div3:=y3-y2; mul4:=x4-x3; div4:=y4-y3;

  for yc:=mny to mxy do begin mnx:=320; mxx:=-1;

      if (y4>=yc) or (y1>=yc) then
        if (y4<=yc) or (y1<=yc) then
          if not(y4=y1) then begin vx:=(yc-y4)*mul1 div div1+x4;
            if vx<mnx then mnx:=vx; if vx>mxx then mxx:=vx; end;

      if (y1>=yc) or (y2>=yc) then
        if (y1<=yc) or (y2<=yc) then
          if not(y1=y2) then begin vx:=(yc-y1)*mul2 div div2+x1;
            if vx<mnx then mnx:=vx; if vx>mxx then mxx:=vx; end;

      if (y2>=yc) or (y3>=yc) then
        if (y2<=yc) or (y3<=yc) then
          if not(y2=y3) then begin vx:=(yc-y2)*mul3 div div3+x2;
            if vx<mnx then mnx:=vx; if vx>mxx then mxx:=vx; end;

      if (y3>=yc) or (y4>=yc) then
        if (y3<=yc) or (y4<=yc) then
          if not(y3=y4) then begin vx:=(yc-y3)*mul4 div div4+x3;
            if vx<mnx then mnx:=vx; if vx>mxx then mxx:=vx; end;

      if mnx<=mxx then FillLine(mnx,yc,mxx,c1,c2) end End;
{-----------------------------------------------------------}
Procedure Zoom(x1,y1,x2,y2,x3,y3,x4,y4:integer);
var vx,vy,vz:integer;
Begin   x2 := x2 + 1; y2 := y2 + 1;
        vx := round((x2-x1)/(x4-x3)*128);
        vy := round((y2-y1)/(y4-y3)*128);
        z  := 320 - x4 + x3;
        y  := y4 - y3; x  := x4 - x3;
Asm     mov ax,y1; shl ax,6; mov bx,ax; shl ax,2; add ax,bx;
        add ax,x1; mov off1,ax; mov ax,y3; shl ax,6; mov bx,ax;
        shl ax,2; add ax,bx; add ax,x3; mov off2,ax; mov es,[source];
        mov dx,off1; xor si,si; mov w2,0;
@LoopY: mov cx,x
@LoopX: mov dx,si; shr dx,7; add dx,off1
        mov di,dx; mov al,es:[di]
        cmp al,0; je @Over
        mov es,[dest]; mov di,[off2]
        mov es:[di],al; mov es,[source]
@Over:  mov ax,vx; add si,ax; inc off2; dec cx
        jnz @LoopX
        xor si,si; mov ax,vy; add w2,ax
        mov ax,w2; shr ax,7; add ax,y1
        shl ax,6; mov bx,ax; shl ax,2
        add ax,bx; add ax,x1; mov off1,ax
        mov ax,z; add off2,ax; dec y
        cmp y,0; jg @LoopY end End;
{-----------------------------------------------------------}
Procedure WriteXY(x1,y1:integer;s:string;c1:byte;c2:shortint);
Begin asm mov ax,y1; shl ax,6; mov bx,ax; shl ax,2
          add ax,bx; add ax,x1; mov off1,ax end;
For z:=1 to length(s) do begin
  for y:=0 to 7 do begin t:=MEM[$FFA6:$E+(Ord(s[z]) shl 3)+y];
    for x:=7 downto 0 do begin
      if t and (1 shl x) <> 0 then MEM[Dest:off1]:=c1; off1:=off1+1;
    end; asm add off1,138h; mov al,c2; add c1,al end;
  end; asm sub off1,9F8h; mov al,c2; shl al,3; sub c1,al end;
end; End;
{-----------------------------------------------------------}
Procedure PPal(p1,c1,rr,gg,bb:byte); Begin
if p1=0 then begin Asm mov dx,3c8h;mov al,[c1];out dx,al;inc dx;
                       mov al,[rr];out dx,al;mov al,[gg];out dx,al;
                       mov al,[bb];out dx,al end end;
if p1=1 then begin Mem[seg(PAL1^):c1 + c1 shl 1]:=rr;
                   Mem[seg(PAL1^):c1 + c1 shl 1 + 1]:=gg;
                   Mem[seg(PAL1^):c1 + c1 shl 1 + 2]:=bb; end;
End;
{-----------------------------------------------------------}
Procedure GPal(p1,c1:byte;var r,g,b:byte);
Var rr,gg,bb : Byte; Begin
if p1=0 then begin asm  mov dx,3c7h; mov al,c1; out dx,al; add dx,2;
                        in al,dx; mov [rr],al; in al,dx; mov [gg],al;
                        in al,dx; mov [bb],al end end;
if p1=1 then begin rr:=Mem[seg(PAL1^):c1 + c1 shl 1];
                   gg:=Mem[seg(PAL1^):c1 + c1 shl 1 + 1];
                   bb:=Mem[seg(PAL1^):c1 + c1 shl 1 + 2]; end;
r:=rr; g:=gg; b:=bb; End;
{-----------------------------------------------------------}
Procedure ShowPal(x1,x2:integer);
Begin for t1:=x1 to x2 do begin w1:=Seg(PAL1^); w2:=t1 + t1 shl 1;
  PPal(0,t1,Mem[w1:w2],Mem[w1:w2+1],Mem[w1:w2+2]) end End;
{-----------------------------------------------------------}
Procedure ScrollPal(x1,x2,x3:integer);
Var wr,wg,wb:byte; Begin w1:=x3; w2:=seg (PAL1^);
For t1:=x1 to x2 do begin tt:=w1 + w1 shl 1;
  w3:=mem[w2:tt]; w4:=mem[w2:tt+1]; w5:=mem[w2:tt+2];
  PPal(0,t1,w3,w4,w5); w1:=w1+1; if w1=x2 then w1:=x1 end End;
{-----------------------------------------------------------}
Procedure FadeTo(x1,x2,x3:integer);
var g,r,b,vg,vr,vb:byte;
Begin GPal(0,x3,vg,vr,vb);
  for t:=x1 to x2 do begin GPal(0,t,g,r,b);
    if g>vg then g:=g-1; if g<vg then g:=g+1;
    if r>vr then r:=r-1; if r<vr then r:=r+1;
    if b>vb then b:=b-1; if b<vb then b:=b+1;
    PPal(0,t,g,r,b) end End;
{-----------------------------------------------------------}
Procedure FadeUp(x1,x2:integer);
var g,r,b,vg,vr,vb:byte;
Begin for t:=x1 to x2 do begin
    GPal(1,t,vg,vr,vb); GPal(0,t,g,r,b);
    if g>vg then g:=g-1; if g<vg then g:=g+1;
    if r>vr then r:=r-1; if r<vr then r:=r+1;
    if b>vb then b:=b-1; if b<vb then b:=b+1;
    PPal(0,t,g,r,b) end End;
{-----------------------------------------------------------}
END.