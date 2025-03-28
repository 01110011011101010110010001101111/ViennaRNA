/**********************************************/
/* BEGIN interface for Inverse Folding a.k.a. */
/* sequence design                            */
/**********************************************/

//%subsection "Inverse Folding"

%rename (inverse_fold) my_inverse_fold;
%{
  char *
  my_inverse_fold(char        *start,
                  const char  *target,
                  float *cost)
  {
    char *seq;
    int n;
    n = strlen(target);
    seq = vrna_random_string(n, symbolset);
    if (start)
      strncpy(seq, start, n);
    *cost = inverse_fold(seq, target);
    if (start)
      /* for backward compatibility modify start */
      strncpy(start, seq, n);
    return(seq);
  }
%}

#ifdef SWIGPYTHON
%feature("autodoc") my_inverse_fold;
%feature("kwargs") my_inverse_fold;
#endif

%newobject my_inverse_fold;
char * my_inverse_fold(char *start, const char *target, float *OUTPUT);

%rename (inverse_pf_fold) my_inverse_pf_fold;
%{
  char *
  my_inverse_pf_fold( char        *start,
                      const char  *target,
                      float       *cost)
  {
    char *seq;
    int n;
    n = strlen(target);
    seq = vrna_random_string(n, symbolset);
    if (start)
      strncpy(seq, start, n);
    *cost = inverse_pf_fold(seq, target);
    if (start)
      /* for backward compatibility modify start */
      strncpy(start, seq, n);
    return(seq);
  }
%}

#ifdef SWIGPYTHON
%feature("autodoc") my_inverse_pf_fold;
%feature("kwargs") my_inverse_pf_fold;
#endif

%newobject my_inverse_pf_fold;
char * my_inverse_pf_fold(char *start, const char *target, float *OUTPUT);

%ignore inverse_fold;
%ignore inverse_pf_fold;


%init %{
/* work around segfault when script tries to free symbolset */

symbolset = (char *) vrna_alloc(sizeof(char) * 5);
strncpy(symbolset, "AUGC", sizeof(char) * 5);

%}

#ifdef SWIGPYTHON
%typemap(varin) char * symbolset {
  free(symbolset);
#if SWIG_VERSION >= 0x040200
  PyObject *bytes = NULL;
  const char *tmp = SWIG_PyUnicode_AsUTF8AndSize($input, NULL, &bytes);
  Py_XDECREF(bytes);
#else
  const char *tmp = SWIG_Python_str_AsChar($input);
#endif
  if ((tmp) &&
      (tmp[0])) {
    symbolset = strdup(tmp);
  } else {
    symbolset = (char *) vrna_alloc(sizeof(char) * 5);
    strncpy(symbolset, "AUGC", sizeof(char) * 5);
  }
}

%typemap(varout) char * symbolset {
  $result = SWIG_Python_str_FromChar((const char *)symbolset);
}
#endif

#ifdef SWIGPERL5
%typemap(varin) char * symbolset {
  free(symbolset);
  symbolset = strdup(SvPV_nolen($input));
}

%typemap(varout) char * symbolset {
  sv_setpv($result, (const char *)symbolset);
}

#endif

%include  <ViennaRNA/inverse/basic.h>

