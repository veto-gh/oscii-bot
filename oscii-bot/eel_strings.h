#ifndef __EEL__STRINGS_H__
#define __EEL__STRINGS_H__

// required for context
// #define EEL_STRING_GET_FOR_INDEX(x, wr) _this->GetStringForIndex(x, wr)
// #define EEL_STRING_GETFMTVAR(x) _this->GetVarForFormat(x)
// #define EEL_STRING_ADDTOTABLE(x)  _this->m_strings.Add(strdup(x.Get()));
// #define EEL_STRING_GETLASTINDEX() (scriptInstance::STRING_INDEX_BASE+_this->m_strings.GetSize() - 1)



/*
  // writeable user-strings are 0..1023 (EEL_STRING_MAX_USER_STRINGS-1), and can be up to about EEL_STRING_MAXUSERSTRING_LENGTH_HINT bytes long

   printf("string %d blah");             -- output to log, allows %d %u %f etc, if host implements formats
   strlen(str);                          -- returns string length
   match("*test*", "this is a test")     -- search for first parameter regex-style in second parameter
   matchi("*test*", "this is a test")    -- search for first parameter regex-style in second parameter (case insensitive)
   strcpy(str, srcstr);                  -- replaces str with srcstr
   strcat(str, srcstr);                  -- appends srcstr to str 
   strcmp(str, str2)                     -- compares strings
   stricmp(str, str2)                    -- compares strings (ignoring case)
   strncmp(str, str2, maxlen)            -- compares strings up to maxlen bytes
   strnicmp(str, str2, maxlen)           -- compares strings (ignoring case) up to maxlen bytes
   strncpy(str, srcstr, maxlen);         -- replaces str with srcstr, up to maxlen (-1 for unlimited)
   strncat(str, srcstr, maxlen);         -- appends up to maxlen of srcstr to str (-1 for unlimited)
   strcpy_from(str,srcstr, offset);      -- copies srcstr to str, but starts reading srcstr at offset offset
   str_getchar(str, offset);             -- returns value at offset offset
   str_setchar(str, offset, value);      -- sets value at offset offset
   str_setlen(str, len);                 -- sets length of string (if increasing, will be space-padded)
                                            (returns previous length)
   str_delsub(str, pos, len);            -- deletes len chars at pos
   str_insert(str, srcstr, pos);         -- inserts srcstr at pos


also recommended, for the PHP fans:

  m_builtin_code = NSEEL_code_compile_ex(m_vm,

   "function strcpy_substr(dest, src, offset, maxlen) ("
   "  offset < 0 ? offset=strlen(src)+offset;"
   "  maxlen < 0 ? maxlen = strlen(src) - offset + maxlen;"
   "  strcpy_from(dest, src, offset);"
   "  maxlen > 0 && strlen(dest) > maxlen ? str_setlen(dest,maxlen);"
   ");\n"


  ,0,NSEEL_CODE_COMPILE_FLAG_COMMONFUNCS

  );



  */


#ifndef EEL_STRING_MAXUSERSTRING_LENGTH_HINT
#define EEL_STRING_MAXUSERSTRING_LENGTH_HINT 16384
#endif

#ifndef EEL_STRING_MAX_USER_STRINGS
#define EEL_STRING_MAX_USER_STRINGS 1024
#endif

#ifndef EEL_STRING_STORAGECLASS 
#define EEL_STRING_STORAGECLASS WDL_FastString
#endif

static int eel_validate_format_specifier(const char *fmt_in, char *typeOut)
{
  const char *fmt = fmt_in+1;
  int state=0;
  if (fmt_in[0] != '%') return 0; // ugh passed a non-specifier
  while (*fmt)
  {
    const char c = *fmt++;
    if (c == 'f'|| c=='e' || c=='E' || c=='g' || c=='G' || c == 'd' || c == 'u' || c == 'x' || c == 'X' || c == 'c' || c =='s' || c=='S') 
    {     
      if (typeOut) *typeOut = c;
      return fmt - fmt_in;
    }
    else if (c == '.') 
    {
      if (state&2) break;
      state |= 2;
    }
    else if (c == '+') 
    {
      if (state&(32|16|8|4)) break;
      state |= 8;
    }
    else if (c == '-') 
    {
      if (state&(32|16|8|4)) break;
      state |= 16;
    }
    else if (c == ' ') 
    {
      if (state&(32|16|8|4)) break;
      state |= 32;
    }
    else if (c >= '0' && c <= '9') 
    {
      state|=4;
    }
    else 
    {
      break; // %unknown-char
    }
  }
  return 0;

}

static bool eel_format_strings(void *opaque, const char *fmt, char *buf, int buf_sz, int want_escapes)
{
  bool rv=true;
  int fmt_parmpos = 0;
  char *op = buf;
  while (*fmt && op < buf+buf_sz-128)
  {
    if (fmt[0] == '%' && fmt[1] == '%') 
    {
      *op++ = '%';
      fmt+=2;
    }
    else if (fmt[0] == '%')
    {
      char ct=0;
      const int l=eel_validate_format_specifier(fmt,&ct);
      char fs[128];
      if (!l || !ct || l >= sizeof(fs)) 
      {
        rv=false;
        break;
      }
      lstrcpyn(fs,fmt,l+1);

      const EEL_F *varptr = EEL_STRING_GETFMTVAR(fmt_parmpos);
      const double v = varptr ? (double)*varptr : 0.0;
      fmt_parmpos++;

      if (ct == 's' || ct=='S')
      {
        const char *str = EEL_STRING_GET_FOR_INDEX(v,NULL);
        snprintf(op,(buf+buf_sz - 3 - op),fs,str ? str : "");
      }
      else if (ct == 'x' || ct == 'X' || ct == 'd' || ct == 'u')
      {
        snprintf(op,64,fs,(int) (v));
      }
      else if (ct == 'c')
      {
        char c = (char) (int)(v);
        if (!c) c=' ';
        snprintf(op,64,fs,c);
      }
      else
        snprintf(op,64,fs,v);

      while (*op) op++;

      fmt += l;
    }
    else 
    {
      if (want_escapes && fmt[0] == '\\')
      {
        const char nc = toupper(fmt[1]);
        if (nc == '\\') { *op++ = '\\'; fmt+=2; }
        else if (nc == 'N')  
        { 
          // (want_escapes & 2) means autoconvert \n to \r\n
          if (want_escapes & 2) *op++ = '\r';
          *op++ = '\n'; fmt+=2; 
        }
        else if (nc == 'R')  
        { 
          // if autoconverting \n to \r\n, filter \r if it is followed by \n
          if (!(want_escapes & 2) || fmt[2] != '\\' || toupper(fmt[3]) != 'N') *op++ = '\r'; 
          fmt+=2; 
        }
        else if (nc == 'T')  { *op++ = '\t'; fmt+=2; }
        else if (nc=='X' || (nc >= '0' && nc <= '9'))
        {
          int base = 10;
          fmt++;
          if (nc == 'X') { fmt++; base=16; }
          else if (nc == '0') base=8;

          int c=0;
          char thisc=toupper(*fmt);
          while ((thisc >= '0' && thisc <= (base>=10 ? '9' : '7')) ||
                 (base == 16 && thisc >= 'A' && thisc <= 'F')
                 )
          {
            c *= base;
            if (thisc >= 'A' && thisc <= 'F')
              c+=thisc - 'A' + 10;
            else
              c += thisc - '0';

            fmt++;
            thisc=toupper(*fmt);
          }
          *op++ = (char)c;
        }
        else *op++ = *fmt++;
      }
      else
      {
        *op++ = *fmt++;
      }
    }

  }
  *op=0;
  return rv;
}



static bool eel_string_match(void *opaque, const char *fmt, const char *msg, int match_fmt_pos, bool ignorecase)
{
  // check for match, updating EEL_STRING_GETFMTVAR(*) as necessary
  // %d=12345
  // %f=12345[.678]
  // %c=any nonzero char, ascii value
  // %x=12354ab
  // %*, %?, %+, %% literals
  // * ? +  match minimal groups of 0+,1, or 1+ chars
  for (;;)
  {
    if (!*fmt && !*msg) return true;

    if (!*fmt) return false; // format ends before matching string

    // if string ends and format is not on a wildcard, early-out to false
    if (!*msg && *fmt != '*') return false;

    switch (*fmt)
    {
      case '*':
      case '+':
        // if last char of search pattern, we're done!
        if (!fmt[1] || (fmt[1] == '?' && !fmt[2])) return *fmt == '*' || *msg;

        if (fmt[0] == '+')  msg++; // skip a character for + . Note that in this case msg[1] is valid, because of the !*msg && *fmt != '*' check above

        fmt++;
        if (*fmt == '?')
        {
          // *? or +? are lazy matches
          fmt++;

          while (*msg && !eel_string_match(opaque,fmt, msg,match_fmt_pos,ignorecase)) msg++;
          return !!*msg;
        }
        else
        {
          // greedy match
          const char *oldmsg = msg;
          while (*msg) msg++;
          while (msg >= oldmsg)
          {
            if (eel_string_match(opaque,fmt, msg,match_fmt_pos,ignorecase))
            {
              return true;
            }
            msg--;
          }
          return false;
        }

      break;
      case '?':
        fmt++;
        msg++;
      break;
      case '%':
        {
          fmt++;
          int fmt_len_parm = 0;
          while (*fmt >= '0' && *fmt <= '9')
          {
            fmt_len_parm *= 10;
            fmt_len_parm += *fmt - '0';
            fmt++;
          }
          const char fmt_char = *fmt;
          if (!fmt_char) return false; // malformed

          fmt++;

          if (fmt_char == '*' || 
              fmt_char == '?' || 
              fmt_char == '+' || 
              fmt_char == '%')
          {
            if (*msg != fmt_char) return false;
            msg++;
          }
          else if (fmt_char == 's')
          {
            match_fmt_pos++;
            // search for a string of min length fmt_len_parm
            const char *oldmsg = msg;

            if (fmt[0] == '?')
            {
              // %s? is lazy mode
              fmt++;
              if ((int)strlen(msg) >= fmt_len_parm)
              {
                msg += fmt_len_parm;
                while (*msg)
                {
                  if (eel_string_match(opaque,fmt, msg,match_fmt_pos,ignorecase)) goto got_string_match;
                  msg++;
                }
              }
            }
            else
            {
              // default to greedy mode
              while (*msg) msg++;

              while (msg >= oldmsg + fmt_len_parm)
              {
                if (eel_string_match(opaque,fmt, msg,match_fmt_pos,ignorecase))
                {
got_string_match:
                  EEL_F *var = EEL_STRING_GETFMTVAR(match_fmt_pos-1);
                  if (var)
                  {
                    EEL_STRING_STORAGECLASS *wr=NULL;
                    EEL_STRING_GET_FOR_INDEX(*var,&wr);
                    if (wr) 
                    {
                      if (msg > oldmsg) wr->Set(oldmsg,msg-oldmsg);
                      else wr->Set("");
                    }
                  }
                  return true;
                }
                msg--;
              }
            }
            return false;
          }
          else if (fmt_char == 'c')
          {
            if (!msg[0]) return false;
            EEL_F *varOut = EEL_STRING_GETFMTVAR(match_fmt_pos);
            if (varOut) *varOut = (EEL_F)msg[0];
            match_fmt_pos++;
            msg++;
          }
          else if (fmt_char == 'd' || fmt_char == 'u')
          {
            int len=0;
            while (msg[len] >= '0' && msg[len] <= '9') len++;
            if (!len) return false;

            EEL_F *varOut = EEL_STRING_GETFMTVAR(match_fmt_pos);
            if (varOut)
            {
              char *bl=(char*)msg;
              if (fmt_char == 'd') 
                *varOut = (EEL_F)atoi(msg);
              else
                *varOut = (EEL_F)strtoul(msg,&bl,10);
            }
            match_fmt_pos++;

            msg+=len;
          }
          else if (fmt_char == 'x' || fmt_char == 'X')
          {
            int len=0;
            while ((msg[len] >= '0' && msg[len] <= '9') ||
                   (msg[len] >= 'A' && msg[len] <= 'F') ||
                   (msg[len] >= 'a' && msg[len] <= 'f')
                   ) len++;
            if (!len) return false;

            EEL_F *varOut = EEL_STRING_GETFMTVAR(match_fmt_pos);
            if (varOut)
            {
              char *bl=(char*)msg;
              *varOut = (EEL_F)strtoul(msg,&bl,16);
            }
            match_fmt_pos++;
            msg+=len;
          }
          else if (fmt_char == 'f')
          {
            int len=0;
            while (msg[len] >= '0' && msg[len] <= '9') len++;
            if (msg[len] == '.') 
            { 
              len++; 
              while (msg[len] >= '0' && msg[len] <= '9') len++;
            }
            if (!len) return false;

            EEL_F *varOut = EEL_STRING_GETFMTVAR(match_fmt_pos);
            if (varOut)
              *varOut = (EEL_F)atof(msg);
            match_fmt_pos++;

            msg+=len;
          }
        }
      break;
      default:
        if (ignorecase ? (toupper(*fmt) != toupper(*msg)) : (*fmt!= *msg)) return false;
        fmt++;
        msg++;
      break;
    }
  }
}



static EEL_F NSEEL_CGEN_CALL _eel_sprintf(void *opaque, EEL_F *strOut, EEL_F *fmt_index)
{
  if (opaque)
  {
    EEL_STRING_STORAGECLASS *wr=NULL;
    EEL_STRING_GET_FOR_INDEX(*strOut, &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"sprintf: bad destination specifier passed %f\n",*strOut);
#endif
    }
    else
    {
      const char *fmt = EEL_STRING_GET_FOR_INDEX(*fmt_index,NULL);
      if (fmt)
      {
        char buf[8192];
        if (eel_format_strings(opaque,fmt,buf,sizeof(buf), 2))
        {
          wr->Set(buf);
          return wr->GetLength();
        }
        else
        {
#ifdef EEL_STRING_DEBUGOUT
          if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"sprintf: bad format string %s\n",fmt);
#endif
        }
      }
      else
      {
#ifdef EEL_STRING_DEBUGOUT
        if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"sprintf: bad format specifier passed %f\n",*fmt_index);
#endif
      }
    }
  }
  return 0.0;
}


static EEL_F NSEEL_CGEN_CALL _eel_strncat(void *opaque, EEL_F *strOut, EEL_F *fmt_index, EEL_F *maxlen)
{
  if (opaque)
  {
    EEL_STRING_STORAGECLASS *wr=NULL;
    EEL_STRING_GET_FOR_INDEX(*strOut, &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"str%scat: bad destination specifier passed %f\n",maxlen ? "n":"",*strOut);
#endif
    }
    else
    {
      const char *fmt = EEL_STRING_GET_FOR_INDEX(*fmt_index,NULL);
      if (fmt)
      {
        if (wr->GetLength() > EEL_STRING_MAXUSERSTRING_LENGTH_HINT)
        {
#ifdef EEL_STRING_DEBUGOUT
          if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"str%scat: will not grow string since it is already %d bytes\n",maxlen ? "n":"",wr->GetLength());
#endif
        }
        else
        {
          int ml=-1;
          if (maxlen && *maxlen >= 0) ml = (int)*maxlen;
          wr->Append(fmt, ml);
          return wr->GetLength();
        }
      }
      else
      {
#ifdef EEL_STRING_DEBUGOUT
        if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"str%scat: bad format specifier passed %f\n",maxlen ? "n":"",*fmt_index);
#endif
      }
    }
  }
  return 0.0;
}

static EEL_F NSEEL_CGEN_CALL _eel_strcpyfrom(void *opaque, EEL_F *strOut, EEL_F *fmt_index, EEL_F *offs)
{
  if (opaque)
  {
    EEL_STRING_STORAGECLASS *wr=NULL;
    EEL_STRING_GET_FOR_INDEX(*strOut, &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"strcpy_from: bad destination specifier passed %f\n",*strOut);
#endif
    }
    else
    {
      const char *fmt = EEL_STRING_GET_FOR_INDEX(*fmt_index,NULL);
      if (fmt)
      {
        int o = (int) *offs;
        if (o < 0) o=0;
        if (o >= (int)strlen(fmt)) wr->Set("");
        else wr->Set(fmt+o);

        return wr->GetLength();
      }
#ifdef EEL_STRING_DEBUGOUT
      if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"strcpy_from: bad format specifier passed %f\n",*fmt_index);
#endif
    }
  }
  return 0.0;
}


static EEL_F NSEEL_CGEN_CALL _eel_strncpy(void *opaque, EEL_F *strOut, EEL_F *fmt_index, EEL_F *maxlen)
{
  if (opaque)
  {
    EEL_STRING_STORAGECLASS *wr=NULL;
    EEL_STRING_GET_FOR_INDEX(*strOut, &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"str%scpy: bad destination specifier passed %f\n",maxlen ? "n":"",*strOut);
#endif
    }
    else
    {
      const char *fmt = EEL_STRING_GET_FOR_INDEX(*fmt_index,NULL);
      if (fmt)
      {
        int ml=-1;
        if (maxlen && *maxlen >= 0) ml = (int)*maxlen;
        wr->Set(fmt,ml);
        return wr->GetLength();
      }
#ifdef EEL_STRING_DEBUGOUT
      if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"str%scpy: bad format specifier passed %f\n",maxlen ? "n":"",*fmt_index);
#endif
    }
  }
  return 0.0;
}


static EEL_F NSEEL_CGEN_CALL _eel_strncmp(void *opaque, EEL_F *aa, EEL_F *bb, EEL_F *maxlen)
{
  if (opaque)
  {
    const char *a = EEL_STRING_GET_FOR_INDEX(*aa,NULL);
    const char *b = EEL_STRING_GET_FOR_INDEX(*bb, NULL);
    if (!a || !b)
    {
#ifdef EEL_STRING_DEBUGOUT
      if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"str%scmp: bad specifier(s) passed %f/%f\n",maxlen ? "n" : "",*aa,*bb);
#endif
    }
    else
    {
      int ml = maxlen ? (int) *maxlen : -1;
      return ml >=0 ? (EEL_F)strncmp(a,b,ml) : (EEL_F) strcmp(a,b);
    }
  }
  return -1.0;
}
static EEL_F NSEEL_CGEN_CALL _eel_strnicmp(void *opaque, EEL_F *aa, EEL_F *bb, EEL_F *maxlen)
{
  if (opaque)
  {
    const char *a = EEL_STRING_GET_FOR_INDEX(*aa,NULL);
    const char *b = EEL_STRING_GET_FOR_INDEX(*bb, NULL);
    if (!a || !b)
    {
#ifdef EEL_STRING_DEBUGOUT
      if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"str%sicmp: bad specifier(s) passed %f/%f\n",maxlen ? "n" : "",*aa,*bb);
#endif
    }
    else
    {
      int ml = maxlen ? (int) *maxlen : -1;
      return ml >=0 ? (EEL_F)strnicmp(a,b,ml) : (EEL_F) stricmp(a,b);
    }
  }
  return -1.0;
}


static EEL_F NSEEL_CGEN_CALL _eel_strcat(void *opaque, EEL_F *strOut, EEL_F *fmt_index)
{
  return _eel_strncat(opaque,strOut,fmt_index,NULL);
}

static EEL_F NSEEL_CGEN_CALL _eel_strcpy(void *opaque, EEL_F *strOut, EEL_F *fmt_index)
{
  return _eel_strncpy(opaque,strOut,fmt_index,NULL);
}


static EEL_F NSEEL_CGEN_CALL _eel_strcmp(void *opaque, EEL_F *strOut, EEL_F *fmt_index)
{
  return _eel_strncmp(opaque,strOut,fmt_index,NULL);
}

static EEL_F NSEEL_CGEN_CALL _eel_stricmp(void *opaque, EEL_F *strOut, EEL_F *fmt_index)
{
  return _eel_strnicmp(opaque,strOut,fmt_index,NULL);
}


static EEL_F NSEEL_CGEN_CALL _eel_strgetchar(void *opaque, EEL_F *strOut, EEL_F *idx)
{
  if (opaque)
  {
    EEL_STRING_STORAGECLASS *wr=NULL;
    EEL_STRING_GET_FOR_INDEX(*strOut, &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"str_getchar: bad destination specifier passed %f\n",*strOut);
#endif
    }
    else
    {
      int l = (int) *idx;
      if (l >= 0 && l < wr->GetLength()) return wr->Get()[l];
    }
  }
  return 0;
}

static EEL_F NSEEL_CGEN_CALL _eel_strsetchar(void *opaque, EEL_F *strOut, EEL_F *idx, EEL_F *val)
{
  if (opaque)
  {
    EEL_STRING_STORAGECLASS *wr=NULL;
    EEL_STRING_GET_FOR_INDEX(*strOut, &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"str_setchar: bad destination specifier passed %f\n",*strOut);
#endif
    }
    else
    {
      int l = (int) *idx;
      int v = (int) *val;
      v &= 255;
      if (l >= 0 && l < wr->GetLength()) 
      {
        if (!v) 
        {
          wr->SetLen(l);
        }
        else
        {
          ((char *)wr->Get())[l]=v;
        }
      }
    }
  }
  return 0;
}

static EEL_F NSEEL_CGEN_CALL _eel_strinsert(void *opaque, EEL_F *strOut, EEL_F *fmt_index, EEL_F *pos)
{
  if (opaque)
  {
    EEL_STRING_STORAGECLASS *wr=NULL;
    EEL_STRING_GET_FOR_INDEX(*strOut, &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"str_insert: bad destination specifier passed %f\n",*strOut);
#endif
    }
    else
    {
      const char *fmt = EEL_STRING_GET_FOR_INDEX(*fmt_index,NULL);
      if (fmt)
      {
        int p = (int)*pos;
        if (p < 0) 
        {
          if ((-p) >= strlen(fmt)) return 0.0;

          fmt += -p;
          p=0;
        }
        int insert_l = strlen(fmt);

        if (insert_l>0)
        {
          if (wr->GetLength() > EEL_STRING_MAXUSERSTRING_LENGTH_HINT)
          {
#ifdef EEL_STRING_DEBUGOUT
            if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"str_insert: will not grow string since it is already %d bytes\n",wr->GetLength());
#endif
            return 0.0;
          }
          wr->Insert(fmt,p);

          return insert_l;
        }
      }
      else
      {
#ifdef EEL_STRING_DEBUGOUT
        if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"str_insert: bad source specifier passed %f\n",*fmt_index);
#endif
      }
    }
  }
  return 0.0;
}

static EEL_F NSEEL_CGEN_CALL _eel_strdelsub(void *opaque, EEL_F *strOut, EEL_F *pos, EEL_F *len)
{
  if (opaque)
  {
    EEL_STRING_STORAGECLASS *wr=NULL;
    EEL_STRING_GET_FOR_INDEX(*strOut, &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"str_delsub: bad destination specifier passed %f\n",*strOut);
#endif
    }
    else
    {
      int p=(int)*pos;
      int l=(int)*len;
      if (p<0)
      {
        l+=p;
        p=0;
      }
      if (l>0)
        wr->DeleteSub(p,l);

      return wr->GetLength();
    }
  }
  return -1.0;
}

static EEL_F NSEEL_CGEN_CALL _eel_strsetlen(void *opaque, EEL_F *strOut, EEL_F *newlen)
{
  if (opaque)
  {
    EEL_STRING_STORAGECLASS *wr=NULL;
    EEL_STRING_GET_FOR_INDEX(*strOut, &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"str_setlen: bad destination specifier passed %f\n",*strOut);
#endif
    }
    else
    {
      int oldlen = wr->GetLength();
      int l = (int) *newlen;
      if (l < 0) l=0;
      if (l > EEL_STRING_MAXUSERSTRING_LENGTH_HINT)
      {
#ifdef EEL_STRING_DEBUGOUT
        EEL_STRING_DEBUGOUT->AppendFormatted(512,"str_setlen: clamping requested length of %d to %d\n",l,EEL_STRING_MAXUSERSTRING_LENGTH_HINT);
#endif
        l=EEL_STRING_MAXUSERSTRING_LENGTH_HINT;
      }
      wr->SetLen(l);

      return oldlen;
    }
  }
  return -1.0;
}


static EEL_F NSEEL_CGEN_CALL _eel_strlen(void *opaque, EEL_F *fmt_index)
{
  if (opaque)
  {
    const char *fmt = EEL_STRING_GET_FOR_INDEX(*fmt_index,NULL);
    if (fmt)
    {
      return strlen(fmt);
    }
  }
  return 0.0;
}




static EEL_F NSEEL_CGEN_CALL _eel_printf(void *opaque, EEL_F *fmt_index)
{
  if (opaque)
  {
    const char *fmt = EEL_STRING_GET_FOR_INDEX(*fmt_index,NULL);
    if (fmt)
    {
      char buf[4096];
      if (eel_format_strings(opaque,fmt,buf,sizeof(buf), 2))
      {
#ifdef EEL_STRING_STDOUT_WRITE
        EEL_STRING_STDOUT_WRITE(buf);
#endif
        return 1.0;
      }
      else
      {
#ifdef EEL_STRING_DEBUGOUT
        if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"printf: bad format string %s\n",fmt);
#endif
      }
    }
    else
    {
#ifdef EEL_STRING_DEBUGOUT
      if (EEL_STRING_DEBUGOUT) EEL_STRING_DEBUGOUT->AppendFormatted(512,"printf: bad format specifier passed %f\n",*fmt_index);
#endif
    }
  }
  return 0.0;
}



static EEL_F NSEEL_CGEN_CALL _eel_match(void *opaque, EEL_F *fmt_index, EEL_F *value_index)
{
  if (opaque)
  {
    const char *fmt = EEL_STRING_GET_FOR_INDEX(*fmt_index,NULL);
    const char *msg = EEL_STRING_GET_FOR_INDEX(*value_index,NULL);

    if (fmt && msg) return eel_string_match(opaque,fmt,msg,0,false) ? 1.0 : 0.0;
  }
  return 0.0;
}
static EEL_F NSEEL_CGEN_CALL _eel_matchi(void *opaque, EEL_F *fmt_index, EEL_F *value_index)
{
  if (opaque)
  {
    const char *fmt = EEL_STRING_GET_FOR_INDEX(*fmt_index,NULL);
    const char *msg = EEL_STRING_GET_FOR_INDEX(*value_index,NULL);

    if (fmt && msg) return eel_string_match(opaque,fmt,msg,0,true) ? 1.0 : 0.0;
  }
  return 0.0;
}

void EEL_string_register()
{
  NSEEL_addfunctionex("strlen",1,(char *)_asm_generic1parm_retd,(char *)_asm_generic1parm_retd_end-(char *)_asm_generic1parm_retd,NSEEL_PProc_THIS,(void *)&_eel_strlen);
  NSEEL_addfunctionex("sprintf",2,(char *)_asm_generic2parm_retd,(char *)_asm_generic2parm_retd_end-(char *)_asm_generic2parm_retd,NSEEL_PProc_THIS,(void *)&_eel_sprintf);

  NSEEL_addfunctionex("strcat",2,(char *)_asm_generic2parm_retd,(char *)_asm_generic2parm_retd_end-(char *)_asm_generic2parm_retd,NSEEL_PProc_THIS,(void *)&_eel_strcat);
  NSEEL_addfunctionex("strcpy",2,(char *)_asm_generic2parm_retd,(char *)_asm_generic2parm_retd_end-(char *)_asm_generic2parm_retd,NSEEL_PProc_THIS,(void *)&_eel_strcpy);
  NSEEL_addfunctionex("strcmp",2,(char *)_asm_generic2parm_retd,(char *)_asm_generic2parm_retd_end-(char *)_asm_generic2parm_retd,NSEEL_PProc_THIS,(void *)&_eel_strcmp);
  NSEEL_addfunctionex("stricmp",2,(char *)_asm_generic2parm_retd,(char *)_asm_generic2parm_retd_end-(char *)_asm_generic2parm_retd,NSEEL_PProc_THIS,(void *)&_eel_stricmp);

  NSEEL_addfunctionex("strncat",3,(char *)_asm_generic3parm_retd,(char *)_asm_generic3parm_retd_end-(char *)_asm_generic3parm_retd,NSEEL_PProc_THIS,(void *)&_eel_strncat);
  NSEEL_addfunctionex("strncpy",3,(char *)_asm_generic3parm_retd,(char *)_asm_generic3parm_retd_end-(char *)_asm_generic3parm_retd,NSEEL_PProc_THIS,(void *)&_eel_strncpy);
  NSEEL_addfunctionex("strncmp",3,(char *)_asm_generic3parm_retd,(char *)_asm_generic3parm_retd_end-(char *)_asm_generic3parm_retd,NSEEL_PProc_THIS,(void *)&_eel_strncmp);
  NSEEL_addfunctionex("strnicmp",3,(char *)_asm_generic3parm_retd,(char *)_asm_generic3parm_retd_end-(char *)_asm_generic3parm_retd,NSEEL_PProc_THIS,(void *)&_eel_strnicmp);
  NSEEL_addfunctionex("str_getchar",2,(char *)_asm_generic2parm_retd,(char *)_asm_generic2parm_retd_end-(char *)_asm_generic2parm_retd,NSEEL_PProc_THIS,(void *)&_eel_strgetchar);
  NSEEL_addfunctionex("str_setlen",2,(char *)_asm_generic2parm_retd,(char *)_asm_generic2parm_retd_end-(char *)_asm_generic2parm_retd,NSEEL_PProc_THIS,(void *)&_eel_strsetlen);
  NSEEL_addfunctionex("strcpy_from",3,(char *)_asm_generic3parm_retd,(char *)_asm_generic3parm_retd_end-(char *)_asm_generic3parm_retd,NSEEL_PProc_THIS,(void *)&_eel_strcpyfrom);
  NSEEL_addfunctionex("str_setchar",3,(char *)_asm_generic3parm_retd,(char *)_asm_generic3parm_retd_end-(char *)_asm_generic3parm_retd,NSEEL_PProc_THIS,(void *)&_eel_strsetchar);
  NSEEL_addfunctionex("str_insert",3,(char *)_asm_generic3parm_retd,(char *)_asm_generic3parm_retd_end-(char *)_asm_generic3parm_retd,NSEEL_PProc_THIS,(void *)&_eel_strinsert);
  NSEEL_addfunctionex("str_delsub",3,(char *)_asm_generic3parm_retd,(char *)_asm_generic3parm_retd_end-(char *)_asm_generic3parm_retd,NSEEL_PProc_THIS,(void *)&_eel_strdelsub);

  NSEEL_addfunctionex("printf",1,(char *)_asm_generic1parm_retd,(char *)_asm_generic1parm_retd_end-(char *)_asm_generic1parm_retd,NSEEL_PProc_THIS,(void *)&_eel_printf);

  NSEEL_addfunctionex("match",2,(char *)_asm_generic2parm_retd,(char *)_asm_generic2parm_retd_end-(char *)_asm_generic2parm_retd,NSEEL_PProc_THIS,(void *)&_eel_match);
  NSEEL_addfunctionex("matchi",2,(char *)_asm_generic2parm_retd,(char *)_asm_generic2parm_retd_end-(char *)_asm_generic2parm_retd,NSEEL_PProc_THIS,(void *)&_eel_matchi);

}

void eel_preprocess_strings(void *opaque, WDL_FastString &procOut, const char *rdptr)
{
  WDL_FastString newstr;
  // preprocess to get strings from "", and replace with an index of someconstant+m_strings.GetSize()
  int comment_state=0; 
  // states:
  // 1 = comment to end of line
  // 2=comment til */
  while (*rdptr)
  {
    const char tc = *rdptr;
    switch (comment_state)
    {
      case 0:
        if (tc == '/')
        {
          if (rdptr[1] == '/') comment_state=1;
          else if (rdptr[1] == '*') comment_state=2;
        }


        if (tc == '"')
        {
          // scan tokens and replace with (idx)
          newstr.Set("");

          rdptr++; 

          int esc_state=0;
          while (*rdptr)
          {
            if (esc_state==0)
            {
              if (*rdptr == '\\') 
              {
                esc_state=1;
              }
              else if (*rdptr == '"')
              {
                if (rdptr[1] != '"') break;
                // "" converts to "
                rdptr++;
              }
            }
            else 
            {
              if (*rdptr != '"') newstr.Append("\\",1);
              esc_state=0;
            }

            if (!esc_state) newstr.Append(rdptr,1);
            rdptr++;
          }

          if (*rdptr) rdptr++; // skip trailing quote

          EEL_STRING_ADDTOTABLE(newstr)
          procOut.AppendFormatted(128,"(%d)",EEL_STRING_GETLASTINDEX());

        }
        else
          procOut.Append(rdptr++,1);

      break;
      case 1:
        if (tc == '\n') comment_state=0;
        procOut.Append(rdptr++,1);
      break;
      case 2:
        if (tc == '*' && rdptr[1] == '/') 
        {
          procOut.Append(rdptr++,1);
          comment_state=0;
        }
        procOut.Append(rdptr++,1);
      break;
    }
  }
}

#endif

