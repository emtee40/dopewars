/* tstring.c      "Translated string" wrappers for dopewars             */
/* Copyright (C)  1998-2000  Ben Webb                                   */
/*                Email: ben@bellatrix.pcl.ox.ac.uk                     */
/*                WWW: http://bellatrix.pcl.ox.ac.uk/~ben/dopewars/     */

/* This program is free software; you can redistribute it and/or        */
/* modify it under the terms of the GNU General Public License          */
/* as published by the Free Software Foundation; either version 2       */
/* of the License, or (at your option) any later version.               */

/* This program is distributed in the hope that it will be useful,      */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        */
/* GNU General Public License for more details.                         */

/* You should have received a copy of the GNU General Public License    */
/* along with this program; if not, write to the Free Software          */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston,               */
/*                   MA  02111-1307, USA.                               */


#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include "dopewars.h"
#include "message.h"
#include "tstring.h"

typedef struct _FmtData {
   union {
      int IntVal;
      price_t PriceVal;
      char CharVal;
      char *StrVal;
   } data;
   char Type;
} FmtData;

gchar *GetTranslatedString(gchar *str,gchar *code,gboolean Caps) {
   gchar *dstr,*pt,*tstr,*Default,*tcode;

   dstr=g_strdup(str);
   g_strdelimit(dstr,"_",'^');
   pt=dstr;
   Default=GetNextWord(&pt,"");
   tstr=NULL;

   while(1) {
      tcode=GetNextWord(&pt,NULL);
      tstr=GetNextWord(&pt,"");
      if (!tcode) { tstr=NULL; break; }
      if (strcmp(tcode,code)==0) {
         break;
      } else tstr=NULL;
   }

   if (tstr) {
      if (Caps) tstr=InitialCaps(tstr); else tstr=g_strdup(tstr);
   } else {
      if (Caps) tstr=InitialCaps(Default); else tstr=g_strdup(Default);
   }

   g_free(dstr);
   return tstr;
}

void tstring_fmt(gchar **tformat,gchar ***tstrings,char *OrigFormat, ...) {
   va_list ap;
   GString *text;
   int i;
   gchar *str,*tstr,code[3],**strings;
   GPtrArray *ptrarr;
   gboolean Caps;

   text=g_string_new("");
   va_start(ap,OrigFormat);
   ptrarr=g_ptr_array_new();

   i=0;
   while (i<strlen(OrigFormat)) {
      g_string_append_c(text,OrigFormat[i]);
      if (OrigFormat[i]=='%') {
         i++;
         if ((OrigFormat[i]=='T' || OrigFormat[i]=='t')
             && i+2<strlen(OrigFormat)) {
            Caps = (OrigFormat[i]=='T');
            code[0]=OrigFormat[i+1];
            code[1]=OrigFormat[i+2];
            code[2]='\0';
            i+=3;
            g_string_append_c(text,'s');
            str=va_arg(ap,char *);
            tstr=GetTranslatedString(str,code,Caps);
            g_ptr_array_add(ptrarr,(gpointer)tstr);
         }
      } else i++;
   }
   va_end(ap);
   *tformat=text->str;
   strings=g_new(char *,ptrarr->len+10);
   for (i=0;i<ptrarr->len;i++) {
      strings[i]=(gchar *)g_ptr_array_index(ptrarr,i);
   }
   strings[ptrarr->len]=NULL;
   g_ptr_array_free(ptrarr,FALSE);
   *tstrings=strings;
   g_string_free(text,FALSE);
}

void tstring_free(gchar *tformat,gchar **tstrings) {
   gchar **pt;
   g_free(tformat);
   for (pt=tstrings;*pt;pt++) g_free(*pt);
   g_free(tstrings);
}

void GetNextFormat(int *Index,gchar *str,int *StartPos,
                   int *EndPos,int *FmtPos,gchar *Type,int *ArgNum,int *Wid,
                   int *Prec,char *Code) {
   int anum,wid,prec,i;
   gchar type;
   *StartPos=-1;
   *EndPos=*FmtPos=*ArgNum=*Wid=*Prec=0;
   *Type=0;
   Code[0]=0;
   anum=wid=prec=0;
   i=*Index;
   while (str[i]) {
      if (str[i]=='%') {
         *StartPos=*EndPos=i++;
         while (strchr("#0- +'",str[i])) i++;  /* Skip flag characters */
         while (str[i]>='0' && str[i]<='9') wid=wid*10+str[i++]-'0';
         if (str[i]=='$') {
            *EndPos=i;
            i++; anum=wid; wid=0;
            while (strchr("#0- +'",str[i])) i++;  /* Skip flag characters */
            while (str[i]>='0' && str[i]<='9') wid=wid*10+str[i++]-'0';
         }
         if (str[i]=='.') {
            i++;
            while (str[i]>='0' && str[i]<='9') prec=prec*10+str[i++]-'0';
         }
         *FmtPos=i;
         type=str[i];
         if ((type=='T' || type=='t') && i+2<strlen(str)) {
            Code[0]=str[i+1];
            Code[1]=str[i+2];
            Code[2]=0;
            i+=3;
         } else i++;
         *ArgNum=anum; *Wid=wid; *Prec=prec; *Index=i; *Type=type;
         return;
      } else i++;
   }
   *Index=i;
}

gchar *HandleTFmt(gchar *format, va_list va) {
   int i,StrInd,StartPos,EndPos,FmtPos,ArgNum,DefaultArgNum,Wid,Prec;
   char Code[3],Type;
   gchar *retstr,*fstr;
   GString *string,*tmpfmt;
   GArray *arr;
   FmtData *fdat;

   string=g_string_new("");
   tmpfmt=g_string_new("");

   arr=g_array_new(FALSE,TRUE,sizeof(FmtData));
   i=DefaultArgNum=0;
   while (i<strlen(format)) {
      GetNextFormat(&i,format,&StartPos,&EndPos,&FmtPos,&Type,&ArgNum,
                    &Wid,&Prec,Code);
      if (StartPos==-1) break;
      if (ArgNum==0) ArgNum=++DefaultArgNum;
      if (ArgNum>arr->len) {
         g_array_set_size(arr,ArgNum);
      }
      g_array_index(arr,FmtData,ArgNum-1).Type=Type;
   }
   for (i=0;i<arr->len;i++) {
      fdat=&g_array_index(arr,FmtData,i);
      switch(fdat->Type) {
         case '\0':
            g_error("Incomplete format string!"); break;
         case 'd':
            fdat->data.IntVal=va_arg(va,int); break;
         case 'P':
            fdat->data.PriceVal=va_arg(va,price_t); break;
         case 'c':
            fdat->data.CharVal=(char)va_arg(va,int); break;
         case 's': case 't': case 'T':
            fdat->data.StrVal=va_arg(va,char *); break;
         default:
            g_error("Unknown format type %c!",fdat->Type);
      }
   }
   i=DefaultArgNum=0;
   while (i<strlen(format)) {
      StrInd=i;
      GetNextFormat(&i,format,&StartPos,&EndPos,&FmtPos,&Type,&ArgNum,
                    &Wid,&Prec,Code);
      if (StartPos==-1) {
         g_string_append(string,&format[StrInd]); break;
      }
      while (StrInd<StartPos) g_string_append_c(string,format[StrInd++]);
      if (ArgNum==0) ArgNum=++DefaultArgNum;
      g_string_assign(tmpfmt,"%");
      EndPos++;
      while (EndPos<FmtPos) g_string_append_c(tmpfmt,format[EndPos++]);
      if (Type=='T' || Type=='t' || Type=='P') g_string_append_c(tmpfmt,'s');
      else g_string_append_c(tmpfmt,Type);
      fdat=&g_array_index(arr,FmtData,ArgNum-1);
      if (Type!=fdat->Type) g_error("Unmatched types!");
      switch(Type) {
         case 'd':
            g_string_sprintfa(string,tmpfmt->str,fdat->data.IntVal); break;
         case 'c':
            g_string_sprintfa(string,tmpfmt->str,fdat->data.CharVal); break;
         case 'P':
            fstr=FormatPrice(fdat->data.PriceVal);
            g_string_sprintfa(string,tmpfmt->str,fstr); g_free(fstr); break;
         case 't': case 'T':
            fstr=GetTranslatedString(fdat->data.StrVal,Code,Type=='T');
            g_string_sprintfa(string,tmpfmt->str,fstr); g_free(fstr); break;
         case 's':
            g_string_sprintfa(string,tmpfmt->str,fdat->data.StrVal); break;
      }
   }
   retstr=string->str;
   g_array_free(arr,TRUE);
   g_string_free(string,FALSE);
   g_string_free(tmpfmt,TRUE);
   return retstr;
}

void dpg_print(gchar *format, ...) {
   va_list ap;
   gchar *retstr;
   va_start(ap,format);
   retstr=HandleTFmt(format,ap);
   va_end(ap);
   g_print(retstr);
   g_free(retstr);
}

gchar *dpg_strdup_printf(gchar *format, ...) {
   va_list ap;
   gchar *retstr;
   va_start(ap,format);
   retstr=HandleTFmt(format,ap);
   va_end(ap);
   return retstr;
}

void dpg_string_sprintf(GString *string, gchar *format, ...) {
   va_list ap;
   gchar *newstr;
   va_start(ap,format);
   newstr=HandleTFmt(format,ap);
   g_string_assign(string,newstr);
   g_free(newstr);
   va_end(ap);
}

void dpg_string_sprintfa(GString *string, gchar *format, ...) {
   va_list ap;
   gchar *newstr;
   va_start(ap,format);
   newstr=HandleTFmt(format,ap);
   g_string_append(string,newstr);
   g_free(newstr);
   va_end(ap);
}