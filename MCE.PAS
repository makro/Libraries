Unit MCE; { Marko`s Compressing Engine v0.01, 6.9.1999 (Under Constraction) }

INTERFACE
Uses Crt;
Var       i,j,k:integer; xx,yy:word;
          Bit,ZBit,PreValue,MaxP,Check:byte;
          GL1,GL2,GS1,GS2,TopGL,TopGS:byte;
          WBitX,RBitX,WByte,RByte:byte;
          WBitXX,RBitXX,CLen,TopLen:word;
          Counter:Array[0..255] of byte;
          ToPP:Array[0..63] of byte;
          ChP,Ch0,Ch1,Ch2,Ch3:byte;
          Done:Boolean;

Procedure Compress(Source,Dest,Size:word);
Procedure UnCompress(Source,Dest,Size:word);
Function  CompressLength:word;
Function  CompressOK:boolean;

IMPLEMENTATION
{--------------------------------------------------------------------------}
Procedure Compress(Source,Dest,Size:word);
{--------------------------------------------------------------------------}
Procedure WBit(Value,Number:byte); Begin Repeat
  if Value and ( 1 shl (Number-1)) <>0 then WByte := WByte + ( 1 shl (WBitX-1));
  Number := Number-1; WBitX := WBitX-1;
  if WBitX=0 then begin Mem[Dest:WBitXX] := WByte;
     WBitXX := WBitXX+1; WBitX := 8; WByte := 0; end;
Until Number=0; End;
{--------------------------------------------------------------------------}
Begin TopLen:=Size;
For Check:=0 to 1 do
  For GS1:=1 to 4 do
    For GL1:=2 to 4 do begin
WBitXX:=0; WBitX:=8; WByte:=0; k:=0; CLen:=0;
if Check=1 then begin GL1:=TopGL; GS1:=TopGS; end;

GS2:= (1 shl (GS1)) shr 1;GL2:=1 shl (GL1);
For i:=0 to 255 do Counter[i]:=0;
For xx:=0 to Size-1 do begin Done:=false;
  if xx>Size-1 then xx:=Size-2;
  if xx>1 then Chp:=Mem[Source:xx-1];
  Ch0:=Mem[Source:xx];Ch1:=Mem[Source:xx+1];
  Ch2:=Mem[Source:xx+2];Ch3:=Mem[Source:xx+3];
  if (Ch0=Ch1) and (Ch0=Ch2) and (Ch0=Ch3) then begin i:=0;
  repeat i:=i+1; Ch1:=Mem[Source:xx+i]; Ch2:=Mem[Source:xx+i+1];
  until (Ch1<>Ch2) or (i>250) or (xx>Size-3); xx:=xx+i; Done:=True; end;
  if not Done then
  if (Ch0>Chp-(GS2-1)) and (Ch0<Chp+GS2) and
     (Ch1>Ch0-(GS2-1)) and (Ch1<Ch0+GS2) then begin i:=-1; k:=0;
  repeat i:=i+1; Ch1:=Mem[Source:xx+i-1]; Ch2:=Mem[Source:xx+i];
  if (Ch2<Ch1-(GS2-2)) or (Ch2>Ch1+(GS2-1)) or (i>GL2) then k:=1;
  if (Ch1=Mem[Source:xx+i+0]) and
     (Ch1=Mem[Source:xx+i+1]) and
     (Ch1=Mem[Source:xx+i+2]) and (i>1) then k:=1;
  until k=1; Done:=True; xx:=xx+i-1; end;
if not Done then Inc(Counter[Ch0]);end;MaxP:=0;
for k:=255 downto 1 do for i:=0 to 255 do
   if (Counter[i]=k) and (MaxP<16) then begin ToPP[MaxP]:=i;
      MaxP:=MaxP+1; end;


  WBitX:=8;WBitXX:=0; WByte:=0;

  WBit(0,8);WBit(255,8); WBit(GL1,4); WBit(GS1,4);
  for i:=0 to 15 do WBit(ToPP[i],8);
{------------------}
  For xx:=0 to Size-1 do begin Done:=false;
  if xx>Size-1 then xx:=Size-2;
  if xx>1 then Chp:=Mem[Source:xx-1];
  Ch0:=Mem[Source:xx];Ch1:=Mem[Source:xx+1];
  Ch2:=Mem[Source:xx+2];Ch3:=Mem[Source:xx+3];
 { RLE }
  if (Ch0=Ch1) and (Ch0=Ch2) and (Ch0=Ch3) then begin i:=0;
  repeat i:=i+1; Ch1:=Mem[Source:xx+i]; Ch2:=Mem[Source:xx+i+1];
  until (Ch1<>Ch2) or (i>250) or (xx>Size-3); xx:=xx+i; Done:=True;
  if i<11 then begin WBit(3,2); WBit(0,1); WBit(i-3,3);  WBit(Ch0,8) end
          else begin WBit(3,2); WBit(1,1); WBit(i-10,8); WBit(Ch0,8) end;end;
 { Glide }
  if not Done then
  if (Ch0>Chp-(GS2-1)) and (Ch0<Chp+GS2) and
     (Ch1>Ch0-(GS2-1)) and (Ch1<Ch0+GS2) then begin i:=-1; k:=0;
  repeat i:=i+1; Ch1:=Mem[Source:xx+i-1]; Ch2:=Mem[Source:xx+i];
  if (Ch2<Ch1-(GS2-2)) or (Ch2>Ch1+(GS2-1)) or (i>GL2) then k:=1;
  if (Ch1=Mem[Source:xx+i+0]) and
     (Ch1=Mem[Source:xx+i+1]) and
     (Ch1=Mem[Source:xx+i+2]) and (i>1) then k:=1;
  until k=1; Done:=True; WBit(2,2); WBit(i-2,GL1);
  k:=i;for i:=0 to k-1 do begin
  Ch1:=Mem[Source:xx+i-1]; Ch2:=Mem[Source:xx+i];
  WBit( (Ch2-Ch1+GS2),GS1); end; xx:=xx+k-1; end;
 { TopWord }
  if not Done then begin
    for i:=0 to 15 do if (Ch0=ToPP[i]) and (not Done) then begin
      WBit(1,2); WBit(i,4); Done:=True; end; end;
 { Normal }
  if not Done then begin WBit(0,2); WBit(Ch0,8); End;
  end; WBit(0,7); CLen:=WBitXX;
  Mem[Dest:0]:=CLen and 255; Mem[Dest:1]:=(Clen shr 8);

 if Check=1 then exit;
 if CLen<TopLen then begin TopGL:=GL1; TopGS:=GS1; TopLen:=CLen; end;
 end; end;
{--------------------------------------------------------------------------}
Procedure UnCompress(Source,Dest,Size:word);
{--------------------------------------------------------------------------}
Procedure RBit(var Value:byte;Number:byte);
Begin Value:=0; Repeat RByte:= Mem[Source:RBitXX];
  if RByte and ( 1 shl (RBitX-1))<>0 then Value := Value + ( 1 shl (Number-1));
  Number := Number-1; RBitX := RBitX-1;
  if RBitX=0 then begin RBitX := 8; RBitXX := RBitXX+1 end;
Until Number=0; End;
{--------------------------------------------------------------------------}
Procedure Put(c:byte);Begin Mem[Dest:xx]:=c; PreValue:=c; End;
{--------------------------------------------------------------------------}
Begin RBitX:=8;RBitXX:=0;
{  RBit(Bit,8);RBit(Bit,8);}
  RBit(Bit,4); GL1:=Bit; GL2:=1 shl GL1;
  RBit(Bit,4); GS1:=Bit; GS2:=(1 shl GS1) shr 1;
  for i:=0 to 15 do begin RBit(Bit,8); ToPP[i]:=Bit; end;

  for xx:=0 to size-1 do begin
  if xx>size then exit;
  RBit(ZBit,2);
 { Normal }
  if ZBit=0 then begin RBit(Bit,8); Put(Bit); end;
 { TopWord }
  if ZBit=1 then begin RBit(Bit,4); Put(ToPP[Bit]); end else
 { Glide }
  if ZBit=2 then Begin RBit(Bit,GL1); i:=Bit+2;
  for k:=0 to i-1 do begin
    RBit(Bit,GS1); Put(PreValue+Bit-GS2); xx:=xx+1; end; xx:=xx-1; end;
 { RLE }
  if ZBit=3 then begin RBit(Bit,1);
      if Bit=0 then begin RBit(Bit,3); i:=Bit+4; end else
                    begin RBit(Bit,8); i:=Bit+11; end; RBit(Bit,8);
      for k:=0 to i-1 do begin Put(Bit); xx:=xx+1; end; xx:=xx-1; end;
 {for}end;

End;
{--------------------------------------------------------------------------}
Function CompressLength:word;Begin CompressLength:=CLen End;
{--------------------------------------------------------------------------}
Function  CompressOK:boolean;Begin End;
{--------------------------------------------------------------------------}
END.
{--------------------------------------------------------------------------}
{
{  How does my compression works?  ... and, here is an answer:
{
{  binarystring              syntax        bytes     Comp.min   Comp.max
{
{  00  <xBYTEx>           : Normal           1       (+25.0%)   (+25.0%)
{  01  xxxx               : TopWord          1       (-25.0%)   (-25.0%)
{  10  xxxx xxxx xxxx...  : Glide          2..16     (-12.5%)   (-45.3%)
{  110 xxx <xBYTEx>       : RLE (< 10)     3..10     (-12.0%)   (-80.6%)
{  111 xxxxxxxx <xBYTEx>  : RLE (>=10)    11..266    (-75.0%)   (-99.1%)
{
{                                     Compressing Rate ( 1.25 - 0.01 )
{                          Aproximate Compressing Rate ( 0.45 - 0.25 )
{                         ( Tested VERY simular graphics screenshots )
{ Syntax:
{  ---    : In the beginning of data there is size of data , one word.
{ Normal  : Just a one stupid byte which doesn`t fit in any other syntax.
{ TopWord : The 15 most often used bytes after Glide+RLE comp...
{ Glide   : Next byte is almoust same as previously -7..+8
{ RLE     : (Run-Length Encoding) ... haven't you ever heard about RLE?
{
{ Better than LBM , PCX , TGA , BMP and PIC for 320x200x256 graphics... !
{ but what about TIF , GIF and PNG? -"Those are not needed... ...ARGH!"
