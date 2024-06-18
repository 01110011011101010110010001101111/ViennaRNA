/* SHAPE reactivity data handling */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>

#include "ViennaRNA/params/default.h"
#include "ViennaRNA/params/constants.h" /* defines MINPSCORE */
#include "ViennaRNA/datastructures/array.h"
#include "ViennaRNA/fold_vars.h"
#include "ViennaRNA/utils/basic.h"
#include "ViennaRNA/utils/strings.h"
#include "ViennaRNA/utils/alignments.h"
#include "ViennaRNA/utils/log.h"
#include "ViennaRNA/io/utils.h"
#include "ViennaRNA/io/file_formats.h"
#include "ViennaRNA/params/basic.h"
#include "ViennaRNA/constraints/soft.h"
#include "ViennaRNA/constraints/SHAPE.h"


#define gaussian(u) (1/(sqrt(2 * PI)) * exp(- u * u / 2))


struct vrna_SHAPE_data_s {
  unsigned int          method;
  vrna_array(double)    params1;
  vrna_array(double)    params2;
  vrna_array(double *)  reactivities;
  vrna_array(double *)  datas1;
  vrna_array(double *)  datas2;
};


/*
 #################################
 # GLOBAL VARIABLES              #
 #################################
 */

/*
 #################################
 # PRIVATE VARIABLES             #
 #################################
 */

/*
 #################################
 # PRIVATE FUNCTION DECLARATIONS #
 #################################
 */
PRIVATE int
apply_Deigan2009_method(vrna_fold_compound_t      *fc,
                        struct vrna_SHAPE_data_s  *data);


PRIVATE int
apply_Zarringhalam2012_method(vrna_fold_compound_t      *fc,
                              struct vrna_SHAPE_data_s  *data);


PRIVATE int
apply_Washietl2012_method(vrna_fold_compound_t      *fc,
                          struct vrna_SHAPE_data_s  *data);


PRIVATE int
apply_Eddy2014_method(vrna_fold_compound_t      *fc,
                      struct vrna_SHAPE_data_s  *data);


PRIVATE void
sc_parse_parameters(const char  *string,
                    char        c1,
                    char        c2,
                    float       *v1,
                    float       *v2);


PRIVATE FLT_OR_DBL
conversion_deigan(double  reactivity,
                  double  m,
                  double  b);


/* PDF of x using Gaussian KDE
 * n is data number
 * h is bandwidth
 */
PRIVATE FLT_OR_DBL
gaussian_kde_pdf(double  x,
                 int     n,
                 float   h,
                 const double* data);


PRIVATE FLT_OR_DBL
exp_pdf(double x,
        double lambda);


/* We use same format as scitpy */
PRIVATE FLT_OR_DBL
gev_pdf(double x,
        double c,
        double loc,
        double scale);

/* Bandwidth for univariate KDE with Scott factor as in scipy */
/* bandwidth = Scott facter * std with ddof = 1 */
PRIVATE FLT_OR_DBL
bandwidth(int n,
          const double* data);

/*
 #################################
 # BEGIN OF FUNCTION DEFINITIONS #
 #################################
 */
PUBLIC int
vrna_sc_SHAPE(vrna_fold_compound_t *fc,
              vrna_SHAPE_data_t    data)
{
  int               ret = 0;

  if ((fc) && (data)) {
    switch (data->method) {
      case VRNA_SHAPE_METHOD_DEIGAN2009:
        ret = apply_Deigan2009_method(fc, data);
        break;

      case VRNA_SHAPE_METHOD_ZARRINGHALAM2012:
        ret = apply_Zarringhalam2012_method(fc, data);
        break;

      case VRNA_SHAPE_METHOD_WASHIETL2012:
        ret = apply_Washietl2012_method(fc, data);
        break;

      case VRNA_SHAPE_METHOD_EDDY2014:
        ret = apply_Eddy2014_method(fc, data);
        break;

      default:
        break;
    }
  }

  return ret;
}


PUBLIC struct vrna_SHAPE_data_s *
vrna_SHAPE_data_Deigan2009(const double *reactivities,
                           unsigned int n,
                           double       m,
                           double       b)
{
  struct vrna_SHAPE_data_s *d = NULL;
  
  if (reactivities)
    d = vrna_SHAPE_data_Deigan2009_comparative(&reactivities,
                                               &n,
                                               1,
                                               &m,
                                               &b);

  return d;
}


PUBLIC struct vrna_SHAPE_data_s *
vrna_SHAPE_data_Deigan2009_comparative(const double       **reactivities,
                                       const unsigned int *n,
                                       unsigned int       n_seq,
                                       double             *ms,
                                       double             *bs)
{
  struct vrna_SHAPE_data_s *d = NULL;
  
  if ((reactivities) &&
      (n) &&
      (ms) &&
      (bs)) {
    d = (struct vrna_SHAPE_data_s *)vrna_alloc(sizeof(struct vrna_SHAPE_data_s));

    d->method = VRNA_SHAPE_METHOD_DEIGAN2009;
    vrna_array_init_size(d->params1, n_seq);
    vrna_array_init_size(d->params2, n_seq);
    vrna_array_init_size(d->reactivities, n_seq);

    for (unsigned int i = 0; i < n_seq; i++) {
      vrna_array_append(d->params1, ms[i]);
      vrna_array_append(d->params2, bs[i]);

      if (reactivities[i]) {
        /* init and store reactivity data */
        vrna_array(FLT_OR_DBL)  a;
        vrna_array_init_size(a, n[i] + 1);
        for (unsigned int j = 0; j <= n[i]; j++)
          vrna_array_append(a, (FLT_OR_DBL)reactivities[i][j]);

        vrna_array_append(d->reactivities, a);
      } else {
        vrna_array_append(d->reactivities, NULL);
      }
    }

    vrna_array_init(d->datas1);
    vrna_array_init(d->datas2);
  }

  return d;
}


PUBLIC struct vrna_SHAPE_data_s *
vrna_SHAPE_data_Zarringhalam2012(const double *reactivities,
                                 unsigned int n,
                                 double       beta,
                                 const char   *pr_conversion,
                                 double       pr_default)
{
  struct vrna_SHAPE_data_s *d = NULL;
  
  if (reactivities) {
    d = (struct vrna_SHAPE_data_s *)vrna_alloc(sizeof(struct vrna_SHAPE_data_s));

    d->method = VRNA_SHAPE_METHOD_ZARRINGHALAM2012;
    vrna_array_init_size(d->params1, 1);
    vrna_array_init(d->params2);
    vrna_array_append(d->params1, beta);

    /* init and store reactivity data */
    vrna_array(FLT_OR_DBL)  a;
    vrna_array_init_size(a, n + 1);
    for (unsigned int i = 0; i <= n; i++)
      vrna_array_append(a, (FLT_OR_DBL)reactivities[i]);

    vrna_array_init_size(d->reactivities, 1);
    vrna_array_append(d->reactivities, a);

    /* prepare probability data according to pr_conversion strategy */
    vrna_array(FLT_OR_DBL)  pr;
    vrna_array_init_size(pr, n + 1);
    for (unsigned int i = 0; i <= n; i++)
      vrna_array_append(pr, (FLT_OR_DBL)reactivities[i]);
    
    vrna_sc_SHAPE_to_pr(pr_conversion, pr, n, pr_default);

    vrna_array_init_size(d->datas1, 1);
    vrna_array_append(d->datas1, pr);


    vrna_array_init(d->datas2);
  }

  return d;
}


PUBLIC struct vrna_SHAPE_data_s *
vrna_SHAPE_data_Zarringhalam2012_comparative(const double **reactivities,
                                             unsigned int *n,
                                             unsigned int n_seq,
                                             double       *betas,
                                             const char   **pr_conversions,
                                             double       *pr_defaults)
{
  struct vrna_SHAPE_data_s *d = NULL;
  
  if (reactivities) {
    d = (struct vrna_SHAPE_data_s *)vrna_alloc(sizeof(struct vrna_SHAPE_data_s));

    d->method = VRNA_SHAPE_METHOD_ZARRINGHALAM2012;
    vrna_array_init_size(d->params1, n_seq);
    vrna_array_init(d->params2);
    vrna_array_init_size(d->reactivities, n_seq);
    vrna_array_init_size(d->datas1, n_seq);

    for (unsigned int i = 0; i < n_seq; i++) {
      vrna_array_append(d->params1, betas[i]);

      if (reactivities[i]) {
        /* init and store reactivity data */
        vrna_array(FLT_OR_DBL)  a;
        vrna_array_init_size(a, n[i] + 1);
        for (unsigned int j = 0; j <= n[i]; j++)
          vrna_array_append(a, (FLT_OR_DBL)reactivities[i][j]);

        vrna_array_append(d->reactivities, a);

        /* prepare probability data according to pr_conversion strategy */
        vrna_array(FLT_OR_DBL)  pr;
        vrna_array_init_size(pr, n[i] + 1);
        for (unsigned int j = 0; j <= n[i]; j++)
          vrna_array_append(pr, (FLT_OR_DBL)reactivities[i][j]);
    
        vrna_sc_SHAPE_to_pr(pr_conversions[i], pr, n[i], pr_defaults[i]);
        vrna_array_append(d->datas1, pr);
      } else {
        vrna_array_append(d->reactivities, NULL);
        vrna_array_append(d->datas1, NULL);
      }
    }

    vrna_array_init(d->datas2);
  }

  return d;
}


PUBLIC void
vrna_SHAPE_data_free(struct vrna_SHAPE_data_s *d)
{
  if (d) {
    /* free all reactivity data */
    for (unsigned int i = 0; i < vrna_array_size(d->reactivities); i++)
      vrna_array_free(d->reactivities[i]);
    vrna_array_free(d->reactivities);

    /* free parameters */
    vrna_array_free(d->params1);
    vrna_array_free(d->params2);

    /* free auxiliary data */
    for (unsigned int i = 0; i < vrna_array_size(d->datas1); i++)
      vrna_array_free(d->datas1[i]);

    vrna_array_free(d->datas1);

    for (unsigned int i = 0; i < vrna_array_size(d->datas2); i++)
      vrna_array_free(d->datas2[i]);

    vrna_array_free(d->datas2);

    free(d);
  }
}


PUBLIC void
vrna_constraints_add_SHAPE(vrna_fold_compound_t *vc,
                           const char           *shape_file,
                           const char           *shape_method,
                           const char           *shape_conversion,
                           int                  verbose,
                           unsigned int         constraint_type)
{
  float             p1, p2;
  char              method;
  char              *sequence;
  double            *values;
  int               i, length = vc->length;
  vrna_SHAPE_data_t d = NULL;

  if (!vrna_sc_SHAPE_parse_method(shape_method, &method, &p1, &p2)) {
    vrna_log_warning("Method for SHAPE reactivity data conversion not recognized!");
    return;
  }

  if (verbose) {
    if (method != 'W') {
      if (method == 'Z') {
        vrna_log_info("Using SHAPE method '%c' with parameter p1=%f", method, p1);
      } else {
        vrna_log_info("Using SHAPE method '%c' with parameters p1=%f and p2=%f",
                          method,
                          p1,
                          p2);
      }
    }
  }

  sequence  = vrna_alloc(sizeof(char) * (length + 1));
  values    = vrna_alloc(sizeof(double) * (length + 1));
  vrna_file_SHAPE_read(shape_file, length, method == 'W' ? 0 : -1, sequence, values);

  switch (method) {
    case 'D':
      d = vrna_SHAPE_data_Deigan2009(values,
                                     length,
                                     p1,
                                     p2);
      break;

    case 'Z':
      d = vrna_SHAPE_data_Zarringhalam2012(values,
                                           length,
                                           p1,
                                           shape_conversion,
                                           0.5);
      break;

    case 'W':
      FLT_OR_DBL *v = vrna_alloc(sizeof(FLT_OR_DBL) * (length + 1));
      for (i = 0; i < length; i++)
        v[i] = values[i];

      vrna_sc_set_up(vc, v, constraint_type);
      free(v);
      free(values);
      free(sequence);

      return;
  }

  (void)vrna_sc_SHAPE(vc, d);
  vrna_SHAPE_data_free(d);

  free(values);
  free(sequence);
}


PUBLIC void
vrna_constraints_add_SHAPE_ali(vrna_fold_compound_t *vc,
                               const char           *shape_method,
                               const char           **shape_files,
                               const int            *shape_file_association,
                               int                  verbose,
                               unsigned int         constraint_type)
{
  float p1, p2;
  char  method;

  if (!vrna_sc_SHAPE_parse_method(shape_method, &method, &p1, &p2)) {
    vrna_log_warning("Method for SHAPE reactivity data conversion not recognized!");
    return;
  }

  if (method != 'D') {
    vrna_log_warning("SHAPE method %c not implemented for comparative prediction!",
                         method);
    vrna_log_warning("Ignoring SHAPE reactivity data!");
    return;
  } else {
    if (verbose) {
      vrna_log_info("Using SHAPE method '%c' with parameters p1=%f and p2=%f",
                        method,
                        p1,
                        p2);
    }

    vrna_sc_add_SHAPE_deigan_ali(vc, shape_files, shape_file_association, p1, p2, constraint_type);
    return;
  }
}


PUBLIC int
vrna_sc_SHAPE_to_pr(const char  *shape_conversion,
                    double      *values,
                    int         length,
                    double      default_value)
{
  int *indices;
  int i, j;
  int index;
  int ret = 1;

  if (!shape_conversion || !(*shape_conversion) || length <= 0)
    return 0;

  if (*shape_conversion == 'S')
    return 1;

  indices = vrna_alloc(sizeof(int) * (length + 1));
  for (i = 1, j = 0; i <= length; ++i) {
    if (values[i] < 0)
      values[i] = default_value;
    else
      indices[j++] = i;
  }

  if (*shape_conversion == 'M') {
    double  max;
    double  map_info[4][2] = { { 0.25, 0.35 },
                               { 0.30, 0.55 },
                               { 0.70, 0.85 },
                               { 0,    1    } };

    max = values[1];
    for (i = 2; i <= length; ++i)
      max = MAX2(max, values[i]);
    map_info[3][0] = max;

    for (i = 0; indices[i]; ++i) {
      double  lower_source  = 0;
      double  lower_target  = 0;

      index = indices[i];

      if (values[index] == 0)
        continue;

      for (j = 0; j < 4; ++j) {
        if (values[index] > lower_source && values[index] <= map_info[j][0]) {
          double  diff_source = map_info[j][0] - lower_source;
          double  diff_target = map_info[j][1] - lower_target;
          values[index] = (values[index] - lower_source) / diff_source * diff_target + lower_target;
          break;
        }

        lower_source  = map_info[j][0];
        lower_target  = map_info[j][1];
      }
    }
  } else if (*shape_conversion == 'C') {
    float cutoff = 0.25;
    int   i;

    sscanf(shape_conversion + 1, "%f", &cutoff);

    for (i = 0; indices[i]; ++i) {
      index         = indices[i];
      values[index] = values[index] < cutoff ? 0 : 1;
    }
  } else if (*shape_conversion == 'L' || *shape_conversion == 'O') {
    int   i;
    float slope     = (*shape_conversion == 'L') ? 0.68 : 1.6;
    float intercept = (*shape_conversion == 'L') ? 0.2 : -2.29;

    sc_parse_parameters(shape_conversion + 1, 's', 'i', &slope, &intercept);

    for (i = 0; indices[i]; ++i) {
      double v;
      index = indices[i];

      v             = (*shape_conversion == 'L') ? values[index] : log(values[index]);
      values[index] = MAX2(MIN2((v - intercept) / slope, 1), 0);
    }
  } else {
    ret = 0;
  }

  free(indices);

  return ret;
}


PUBLIC int
vrna_sc_add_SHAPE_zarringhalam(vrna_fold_compound_t *vc,
                               const double         *reactivities,
                               double               b,
                               double               default_value,
                               const char           *shape_conversion,
                               unsigned int         options)
{
  int ret;

  ret = 0; /* error */

  if ((vc) &&
      (reactivities && (vc->type == VRNA_FC_TYPE_SINGLE))) {
    vrna_SHAPE_data_t d = vrna_SHAPE_data_Zarringhalam2012(reactivities,
                                                           vc->length,
                                                           b,
                                                           shape_conversion,
                                                           default_value);
    ret = vrna_sc_SHAPE(vc, d);
    vrna_SHAPE_data_free(d);
  }

  return ret;
}


PUBLIC int
vrna_sc_add_SHAPE_deigan(vrna_fold_compound_t *vc,
                         const double         *reactivities,
                         double               m,
                         double               b,
                         unsigned int         options)
{
  int         ret = 0;

  if ((vc) &&
      (reactivities)) {
    switch (vc->type) {
      case VRNA_FC_TYPE_SINGLE:
        vrna_SHAPE_data_t d = vrna_SHAPE_data_Deigan2009(reactivities, vc->length, m, b);
        ret = vrna_sc_SHAPE(vc, d);
        vrna_SHAPE_data_free(d);
        break;

      case VRNA_FC_TYPE_COMPARATIVE:
        vrna_log_warning("vrna_sc_add_SHAPE_deigan() not implemented for comparative prediction! "
                             "Use vrna_sc_add_SHAPE_deigan_ali() instead!");
        break;
    }
  }

  return ret;
}


PUBLIC int
vrna_sc_add_SHAPE_deigan_ali(vrna_fold_compound_t *vc,
                             const char           **shape_files,
                             const int            *shape_file_association,
                             double               m,
                             double               b,
                             unsigned int         options)
{
  FILE          *fp;
  float         reactivity, *reactivities, weight;
  char          *line, nucleotide, *sequence;
  int           s, i, r, n_data, position, n_seq, ret;
  FLT_OR_DBL    **contributions, energy;
  unsigned int  **a2s;

  ret = 0;

  if (vc && (vc->type == VRNA_FC_TYPE_COMPARATIVE)) {
    n_seq = vc->n_seq;
    a2s   = vc->a2s;

    vrna_sc_init(vc);

    /* count number of SHAPE data available for this alignment */
    for (n_data = s = 0; shape_file_association[s] != -1; s++) {
      if (shape_file_association[s] >= n_seq)
        continue;

      /* try opening the shape data file */
      if ((fp = fopen(shape_files[s], "r"))) {
        fclose(fp);
        n_data++;
      }
    }

    weight = (n_data > 0) ? ((float)n_seq / (float)n_data) : 0.;

    /* collect contributions for the sequences in the alignment */
    contributions = (FLT_OR_DBL **)vrna_alloc(sizeof(FLT_OR_DBL *) * (n_seq));

    for (s = 0; shape_file_association[s] != -1; s++) {
      int ss = shape_file_association[s]; /* actual sequence number in alignment */

      if (ss >= n_seq) {
        vrna_log_warning("Failed to associate SHAPE file \"%s\" with sequence %d in alignment! "
                             "Alignment has only %d sequences!",
                             shape_files[s],
                             ss,
                             n_seq);
        continue;
      }

      /* read the shape file */
      if (!(fp = fopen(shape_files[s], "r"))) {
        vrna_log_warning("Failed to open SHAPE data file \"%d\"! "
                             "No shape data will be used for sequence %d.",
                             s,
                             ss + 1);
      } else {
        reactivities  = (float *)vrna_alloc(sizeof(float) * (vc->length + 1));
        sequence      = (char *)vrna_alloc(sizeof(char) * (vc->length + 1));

        /* initialize reactivities with missing data for entire alignment length */
        for (i = 1; i <= vc->length; i++)
          reactivities[i] = -1.;

        while ((line = vrna_read_line(fp))) {
          r = sscanf(line, "%d %c %f", &position, &nucleotide, &reactivity);
          if (r) {
            if (position <= 0) {
              vrna_log_warning("SHAPE data for position %d outside alignment!", position);
            } else if (position > vc->length) {
              vrna_log_warning("SHAPE data for position %d outside alignment!", position);
            } else {
              switch (r) {
                case 1:
                  nucleotide = 'N';
                /* fall through */
                case 2:
                  reactivity = -1.;
                /* fall through */
                default:
                  sequence[position - 1]  = nucleotide;
                  reactivities[position]  = reactivity;
                  break;
              }
            }
          }

          free(line);
        }
        fclose(fp);

        sequence[vc->length] = '\0';

        /* double check information by comparing the sequence read from */
        char *tmp_seq = vrna_seq_ungapped(vc->sequences[shape_file_association[s]]);
        if (strcmp(tmp_seq, sequence))
          vrna_log_warning("Input sequence %d differs from sequence provided via SHAPE file!",
                               shape_file_association[s] + 1);

        free(tmp_seq);

        /*  begin preparation of the pseudo energies */
        /*  beware of the fact that energy_stack will be accessed through a2s[s] array,
         *  hence pseudo_energy might be gap-free (default)
         */
        int gaps, is_gap;
        contributions[ss] = (FLT_OR_DBL *)vrna_alloc(sizeof(FLT_OR_DBL) * (vc->length + 1));
        for (gaps = 0, i = 1; i <= vc->length; i++) {
          is_gap  = (vc->sequences[ss][i - 1] == '-') ? 1 : 0;
          energy  =
            ((i - gaps > 0) && !(is_gap)) ? conversion_deigan(reactivities[i - gaps], m,
                                                              b) * weight : 0.;

          if (vc->params->model_details.oldAliEn)
            contributions[ss][i] = energy;
          else if (!is_gap)
            contributions[ss][a2s[ss][i]] = energy;

          gaps += is_gap;
        }

        free(reactivities);
      }
    }

    ret = vrna_sc_set_stack_comparative(vc, (const FLT_OR_DBL **)contributions, options);

    for (s = 0; s < n_seq; s++)
      free(contributions[s]);

    free(contributions);
  }

  return ret;
}


PUBLIC int
vrna_sc_SHAPE_parse_method(const char *method_string,
                           char       *method,
                           float      *param_1,
                           float      *param_2)
{
  const char *params = method_string + 1;

  *param_1  = 0;
  *param_2  = 0;

  if (!method_string || !method_string[0])
    return 0;

  *method = method_string[0];

  switch (method_string[0]) {
    case 'Z':
      *param_1 = 0.89;
      sc_parse_parameters(params, 'b', '\0', param_1, NULL);
      break;

    case 'D':
      *param_1  = 1.8;
      *param_2  = -0.6;
      sc_parse_parameters(params, 'm', 'b', param_1, param_2);
      break;

    case 'W':
      break;

    default:
      *method = 0;
      return 0;
  }

  return 1;
}


PUBLIC int
vrna_sc_add_SHAPE_eddy_2(vrna_fold_compound_t *fc,
                         const double         *reactivities,
                         int                  unpaired_nb,
                         const double         *unpaired_data,
                         int                  paired_nb,
                         const double         *paired_data)
{
  int i, j;
  double kT;
  /* FLT_OR_DBL  *unpaired_values; */
  FLT_OR_DBL  *paired_values;
  float unpaired_h, paired_h;

  kT = GASCONST * ((fc->params)->temperature + K0) / 1000; /* in kcal/mol */

  if ((fc) &&
      (reactivities)) {
    switch (fc->type) {
      case VRNA_FC_TYPE_SINGLE:
        /* unpaired_values = (FLT_OR_DBL *)vrna_alloc(sizeof(FLT_OR_DBL) * (fc->length + 1)); */
        paired_values = (FLT_OR_DBL *)vrna_alloc(sizeof(FLT_OR_DBL) * (fc->length + 1));

        /* Compute bandwidth */
        unpaired_h = bandwidth(unpaired_nb, unpaired_data);
        paired_h = bandwidth(paired_nb, paired_data);

        /* convert and add */
        for (i = 1; i <= fc->length; ++i) {
          /* add for unpaired position */
          vrna_sc_add_up(fc, i, - kT * log(gaussian_kde_pdf(reactivities[i], unpaired_nb, unpaired_h, unpaired_data)), VRNA_OPTION_DEFAULT);
          paired_values[i] = - kT * log(gaussian_kde_pdf(reactivities[i], paired_nb, paired_h, paired_data));
        }

        for (i = 1; i <= fc->length; ++i) {
          for (j = i + 1; j <= fc->length; ++j)
            vrna_sc_add_bp(fc, i, j, paired_values[i] + paired_values[j], VRNA_OPTION_DEFAULT);
        }
        /* always store soft constraints in plain format */
        free(paired_values);

        return 1; /* success */

      case VRNA_FC_TYPE_COMPARATIVE:
        vrna_message_warning("vrna_sc_add_SHAPE_eddy_2() not implemented for comparative prediction! ");
        break;
    }
  }
  return 0;
}


PRIVATE int
apply_Deigan2009_method(vrna_fold_compound_t      *fc,
                        struct vrna_SHAPE_data_s  *data)
{
  unsigned int  i, s, n, n_data, **a2s;
  int           ret;
  FLT_OR_DBL    energy, *vs, **cvs;

  ret = 0;
  n   = fc->length;

  switch (fc->type) {
    case VRNA_FC_TYPE_SINGLE:
      if ((vrna_array_size(data->reactivities) > 0) &&
          (n <= vrna_array_size(data->reactivities[0]))) {
        vs = (FLT_OR_DBL *)vrna_alloc(sizeof(FLT_OR_DBL) * (n + 1));

        /* first convert the values according to provided slope and intercept values */
        for (i = 1; i <= n; ++i)
          vs[i] = conversion_deigan(data->reactivities[0][i], data->params1[0], data->params2[0]);

        /* always store soft constraints in plain format */
        ret = vrna_sc_set_stack(fc, (const FLT_OR_DBL *)vs, VRNA_OPTION_DEFAULT);

        free(vs);
      }
      break;

    case VRNA_FC_TYPE_COMPARATIVE:
      if (vrna_array_size(data->reactivities) >= fc->n_seq) {
        a2s = fc->a2s;

        /* collect contributions for the sequences in the alignment */
        cvs = (FLT_OR_DBL **)vrna_alloc(sizeof(FLT_OR_DBL *) * (fc->n_seq));

        for (n_data = s = 0; s < fc->n_seq; s++)
          if (data->reactivities[s] != NULL)
            n_data++;

        FLT_OR_DBL weight = (n_data > 0) ? ((FLT_OR_DBL)fc->n_seq / (FLT_OR_DBL)n_data) : 0.;

        for (s = 0; s < fc->n_seq; s++) {
          if (data->reactivities[s] != NULL) {
            /*  begin preparation of the pseudo energies */
            /*  beware of the fact that energy_stack will be accessed through a2s[s] array,
             *  hence pseudo_energy might be gap-free (default)
             */
            unsigned int gaps, is_gap;
            cvs[s] = (FLT_OR_DBL *)vrna_alloc(sizeof(FLT_OR_DBL) * (n + 1));
            for (gaps = 0, i = 1; i <= n; i++) {
              is_gap  = (fc->sequences[s][i - 1] == '-') ? 1 : 0;
              energy  = 0;
              if ((i - gaps > 0) && !(is_gap))
                energy = conversion_deigan(data->reactivities[s][i - gaps],
                                           data->params1[s],
                                           data->params2[s]) * weight;

              if (fc->params->model_details.oldAliEn)
                cvs[s][i] = energy;
              else if (!is_gap)
                cvs[s][a2s[s][i]] = energy;

              gaps += is_gap;
            }
          }
        }

        ret = vrna_sc_set_stack_comparative(fc, (const FLT_OR_DBL **)cvs, VRNA_OPTION_DEFAULT);
      }
      break;
  }
  
  return ret;
}


PRIVATE int
apply_Zarringhalam2012_method(vrna_fold_compound_t      *fc,
                              struct vrna_SHAPE_data_s  *data)
{
  unsigned int  i, j, n;
  int           ret;
  FLT_OR_DBL    *up, **bp;
  double        *pr, b;

  n   = fc->length;
  ret = 0;
  switch (fc->type) {
    case VRNA_FC_TYPE_SINGLE:
      if ((vrna_array_size(data->datas1) > 0) &&
          (n <= vrna_array_size(data->datas1[0]))) {
        /*  now, convert probabilities into pseudo free energies for unpaired, and
         *  paired nucleotides
         */
        pr  = data->datas1[0];
        b   = data->params1[0];

        up  = (FLT_OR_DBL *)vrna_alloc(sizeof(FLT_OR_DBL) * (n + 1));
        bp  = (FLT_OR_DBL **)vrna_alloc(sizeof(FLT_OR_DBL *) * (n + 1));

        for (i = 1; i <= n; ++i) {
          bp[i] = (FLT_OR_DBL *)vrna_alloc(sizeof(FLT_OR_DBL) * (n + 1));
          up[i] = b * fabs(pr[i] - 1);

          for (j = i + 1; j <= n; ++j)
            bp[i][j] = b * (pr[i] + pr[j]);
        }

        /* add the pseudo energies as soft constraints */
        vrna_sc_set_up(fc, (const FLT_OR_DBL *)up, VRNA_OPTION_DEFAULT);
        vrna_sc_set_bp(fc, (const FLT_OR_DBL **)bp, VRNA_OPTION_DEFAULT);

        /* clean up memory */
        for (i = 1; i <= n; ++i)
          free(bp[i]);
        free(bp);
        free(up);

        ret = 1; /* success */
      }
      break;

    case VRNA_FC_TYPE_COMPARATIVE:
      if (vrna_array_size(data->reactivities) >= fc->n_seq) {
        
      }
      break;
  }

  return ret;
}


PRIVATE int
apply_Washietl2012_method(vrna_fold_compound_t      *fc,
                          struct vrna_SHAPE_data_s  *data)
{
  int ret;

  ret = 0;
  switch (fc->type) {
    case VRNA_FC_TYPE_SINGLE:
      break;

    case VRNA_FC_TYPE_COMPARATIVE:
      break;
  }

  return ret;
}


PRIVATE int
apply_Eddy2014_method(vrna_fold_compound_t      *fc,
                      struct vrna_SHAPE_data_s  *data)
{
  int ret = 0;

  return ret;
}



PRIVATE void
sc_parse_parameters(const char  *string,
                    char        c1,
                    char        c2,
                    float       *v1,
                    float       *v2)
{
  char        *fmt;
  const char  warning[] = "SHAPE method parameters not recognized! Using default parameters!";
  int         r;

  assert(c1);
  assert(v1);

  if (!string || !(*string))
    return;

  if (c2 == 0 || v2 == NULL) {
    fmt = vrna_strdup_printf("%c%%f", c1);
    r   = sscanf(string, fmt, v1);

    if (!r)
      vrna_log_warning(warning);

    free(fmt);

    return;
  }

  fmt = vrna_strdup_printf("%c%%f%c%%f", c1, c2);
  r   = sscanf(string, fmt, v1, v2);

  if (r != 2) {
    free(fmt);
    fmt = vrna_strdup_printf("%c%%f", c1);
    r   = sscanf(string, fmt, v1);

    if (!r) {
      free(fmt);
      fmt = vrna_strdup_printf("%c%%f", c2);
      r   = sscanf(string, fmt, v2);

      if (!r)
        vrna_log_warning(warning);
    }
  }

  free(fmt);
}


PRIVATE FLT_OR_DBL
conversion_deigan(double  reactivity,
                  double  m,
                  double  b)
{
  return reactivity < 0 ? 0. : (FLT_OR_DBL)(m * log(reactivity + 1) + b);
}


/******************/
/*      Eddy      */
/******************/

PRIVATE FLT_OR_DBL
gaussian_kde_pdf(double  x,
                 int     n,
                 float   h,
                 const double* data)
{
  FLT_OR_DBL total;
  total = 0.;
  for (int i = 0; i < n; i++)
    total += gaussian((x - data[i]) / h);
  return total / (n * h);
}


PRIVATE FLT_OR_DBL
exp_pdf(double x,
        double lambda)
{
  return x < 0 ? 0. : (FLT_OR_DBL)(lambda * exp(- lambda * x));
}


PRIVATE FLT_OR_DBL
gev_pdf(double x,
        double c,
        double loc,
        double scale)
{
  FLT_OR_DBL s, t;
  s = (FLT_OR_DBL) ((x - loc) / scale);
  t = c * s;
  
  if (c == 0) {
    return exp(-s) * exp(-exp(-s));
  } else if (t < 1) {
    return pow(1 - t, 1 / c - 1) * exp(-pow(1 - t, 1 / c));
  } else {
    return 0;
  }
}


PRIVATE FLT_OR_DBL
bandwidth(int n,
          const double* data)
{
  double factor, mu, std;
  mu = 0.;
  std = 0.;

  factor = (pow(n, -1./5));

  for (int i = 0; i < n; i++)
    mu += data[i];
  mu /= n;

  for (int i = 0; i < n; i++)
    std += (data[i] - mu) * (data[i] - mu);
  std = sqrt(std / (n - 1));
  return (FLT_OR_DBL)(factor * std);
}
