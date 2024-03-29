{
  bsm2txt v. 1.0
  (c) Viktor Pykhonin <pyk@mail.ru>, 2016
 
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
}


program bas2txt;

uses SysUtils;

const symtable:ansistring =' !"#$%&''()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_ž€–„…”ƒ•ˆ‰Š‹ŒŽŸ‘’“†‚œ›‡˜™—Û';

const keywords:array[0..91] of string = ('CLS','FOR','NEXT','DATA','INPUT','DIM','READ','CUR','GOTO','RUN','IF','RESTORE','GOSUB','RETURN','REM',
                  'STOP','OUT','ON','PLOT','LINE','POKE','PRINT','DEF','CONT','LIST','CLEAR','CLOAD','CSAVE','NEW','TAB(',
                  'TO','SPC(','FN','THEN','NOT','STEP','+','-','*','/','^','AND','OR','>','=','<','SGN','INT','ABS','USR',
                  'FRE','INP','POS','SQR','RND','LOG','EXP','COS','SIN','TAN','ATN','PEEK','LEN','STR$','VAL','ASC','CHR$',
                  'LEFT$','RIGHT$','MID$','SCREEN$(','INKEY$','AT','&','BEEP','PAUSE','VERIFY','HOME','EDIT','DELETE',
                  'MERGE','AUTO','HIMEM','@','ASN','ADDR','PI','RENUM','ACS','LG','LPRINT','LLIST');

var f: file of byte;
    b1, b2: byte;
    num: integer;

begin
  if ParamStr(1) = '' then
  begin
       writeln('Bsm2Txt v. 1.0, (c) Viktor Pykhonin');
       writeln('Basic Micron to text converter utility');
       writeln;
       writeln('Usage: bsm2txt filename.bsm > outfile');
       exit();
  end;

  assignfile(f, ParamStr(1));
  reset(f);
  repeat
      read(f, b1);
  until b1<>$d3;
  if b1<>0 then
  begin
    repeat
        read(f, b1);
    until b1=$e6;
    repeat
        read(f, b1);
    until b1<>$d3;
  end;

  while true do
  begin
       read(f,b1);
       if b1=0 then
          break;
       read(f,b2);

       read(f,b1);
       read(f,b2);
       num:=b1+b2*256;

       write(num,' ');

       repeat
             read(f,b1);
             if (b1<$80) then
             begin
                  if b1>=$20 then
                      write(symtable[b1-$1f])
                  else if b1<>0 then
                    write('?')
             end else
                 write(keywords[b1-$80]);
       until b1=0;

       writeln;
  end;

  close(f);

end.

