/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2018-2019 Rajit Manohar
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 **************************************************************************
 */
#include <act/types.h>
#include <act/lang.h>
#include <act/inst.h>
#include <act/body.h>
#include <act/value.h>
#include "prs.h"
#include "qops.h"
#include "config.h"
#include <string.h>

act_prs_lang_t *prs_expand (act_prs_lang_t *, ActNamespace *, Scope *);

act_size_spec_t *act_expand_size (act_size_spec_t *sz, ActNamespace *ns, Scope *s);

static ActId *fullexpand_var (ActId *id, ActNamespace *ns, Scope *s)
{
  ActId *idtmp;
  Expr *etmp;
  
  if (!id) return NULL;
  
  idtmp = id->Expand (ns, s);
  
  etmp = idtmp->Eval (ns, s);
  Assert (etmp->type == E_VAR, "Hmm");
  if ((ActId *)etmp->u.e.l != idtmp) {
    delete idtmp;
  }
  idtmp = (ActId *)etmp->u.e.l;
  /* check that type is a bool */
  FREE (etmp);
  return idtmp;
}

/*
 * This is a chp variable to be read. So it can have array
 * dereferences
 */
static ActId *expand_var_read (ActId *id, ActNamespace *ns, Scope *s)
{
  ActId *idtmp;
  Expr *etmp;
  
  if (!id) return NULL;

  /* this needs to be changed  to: expand as much as possible, not
     full expand. It needs to be a non-array variable type.
  */
  idtmp = id->Expand (ns, s);
  
  etmp = idtmp->Eval (ns, s);
  Assert (etmp->type == E_VAR, "Hmm");
  if ((ActId *)etmp->u.e.l != idtmp) {
    delete idtmp;
  }
  idtmp = (ActId *)etmp->u.e.l;
  /* check that type is a bool */
  FREE (etmp);
  return idtmp;
}

static ActId *expand_var_write (ActId *id, ActNamespace *ns, Scope *s)
{
  return expand_var_read (id, ns, s);
}

static ActId *expand_var_chan (ActId *id, ActNamespace *ns, Scope *s)
{
  return expand_var_read (id, ns, s);
}

void _merge_duplicate_rules (act_prs_lang_t *p)
{
  /* XXX: fix this */
}

act_prs *prs_expand (act_prs *p, ActNamespace *ns, Scope *s)
{
  act_prs *ret;

  if (!p) return NULL;

  NEW (ret, act_prs);

  ret->vdd = fullexpand_var (p->vdd, ns, s);
  ret->gnd = fullexpand_var (p->gnd, ns, s);
  ret->psc = fullexpand_var (p->psc, ns, s);
  ret->nsc = fullexpand_var (p->nsc, ns, s);
  ret->p = prs_expand (p->p, ns, s); 
  _merge_duplicate_rules (ret->p);
 
  if (p->next) {
    ret->next = prs_expand (p->next, ns, s);
  }
  else {
    ret->next = NULL;
  }

  return ret;
}


act_prs_expr_t *prs_expr_expand (act_prs_expr_t *p, ActNamespace *ns, Scope *s)
{
  int pick;
  
  act_prs_expr_t *ret = NULL;
  if (!p) return NULL;

  NEW (ret, act_prs_expr_t);
  ret->type = p->type;
  switch (p->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    ret->u.e.l = prs_expr_expand (p->u.e.l, ns, s);
    ret->u.e.r = prs_expr_expand (p->u.e.r, ns, s);
    ret->u.e.pchg = prs_expr_expand (p->u.e.pchg, ns, s);
    ret->u.e.pchg_type = p->u.e.pchg_type;

    pick = -1;

    if (ret->u.e.l->type == ACT_PRS_EXPR_TRUE) {
      /* return ret->u.e.r; any pchg is skipped */
      if (p->type == ACT_PRS_EXPR_AND) {
	pick = 0; /* RIGHT */
      }
      else {
	pick = 1; /* LEFT */
      }
    }
    else if (ret->u.e.l->type == ACT_PRS_EXPR_FALSE) {
	if (p->type == ACT_PRS_EXPR_AND) {
	  pick = 1; /* LEFT */
	}
	else {
	  pick = 0; /* RIGHT */
	}
    }
    else if (ret->u.e.r->type == ACT_PRS_EXPR_TRUE) {
      if (p->type == ACT_PRS_EXPR_AND) {
	pick = 1; /* LEFT */
      }
      else {
	pick = 0; /* RIGHT */
      }
    }
    else if (ret->u.e.r->type == ACT_PRS_EXPR_FALSE) {
      if (p->type == ACT_PRS_EXPR_AND) {
	pick = 0; /* RIGHT */
      }
      else {
	pick = 1; /* LEFT */
      }
    }
    if (pick == 0) {
      act_prs_expr_t *t;
      /* RIGHT */
      act_free_a_prs_expr (ret->u.e.l);
      act_free_a_prs_expr (ret->u.e.pchg);
      t = ret->u.e.r;
      FREE (ret);
      ret = t;
    }
    else if (pick == 1) {
      act_prs_expr_t *t;
      /* LEFT */
      act_free_a_prs_expr (ret->u.e.r);
      act_free_a_prs_expr (ret->u.e.pchg);
      t = ret->u.e.l;
      FREE (ret);
      ret = t;
    }
    break;

  case ACT_PRS_EXPR_NOT:
    ret->u.e.l = prs_expr_expand (p->u.e.l, ns, s);
    ret->u.e.r = NULL;
    if (ret->u.e.l->type == ACT_PRS_EXPR_FALSE) {
      act_prs_expr_t *t;
      ret->u.e.l->type = ACT_PRS_EXPR_TRUE;
      t = ret->u.e.l;
      FREE (ret);
      ret = t;
    }
    else if (ret->u.e.l->type == ACT_PRS_EXPR_TRUE) {
      act_prs_expr_t *t;
      ret->u.e.l->type = ACT_PRS_EXPR_FALSE;
      t = ret->u.e.l;
      FREE (ret);
      ret = t;
    }
    break;

  case ACT_PRS_EXPR_LABEL:
    ret->u.l.label = p->u.l.label;
    break;

  case ACT_PRS_EXPR_VAR:
    ret->u.v.sz = act_expand_size (p->u.v.sz, ns, s);
    ret->u.v.id = expand_var_read (p->u.v.id, ns, s);
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    Assert (s->Add (p->u.loop.id, TypeFactory::Factory()->NewPInt()),
	    "Huh?");
    {
      ValueIdx *vx;
      int ilo, ihi, i;
      Expr *etmp;

      if (p->u.loop.lo) {
	etmp = expr_expand (p->u.loop.lo, ns, s);
	if (etmp->type != E_INT) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "Expanding loop range in prs body\n  expr: ");
	  print_expr (stderr, p->u.loop.lo);
	  fprintf (stderr, "\nNot a constant int\n");
	  exit (1);
	}
	ilo = etmp->u.v;
	//FREE (etmp);
      }
      else {
	ilo = 0;
      }

      etmp = expr_expand (p->u.loop.hi, ns, s);
      if (etmp->type != E_INT) {
	act_error_ctxt (stderr);
	fprintf (stderr, "Expanding loop range in prs body\n  expr: ");
	print_expr (stderr, p->u.loop.hi);
	fprintf (stderr, "\nNot a constant int\n");
	exit (1);
      }
      if (p->u.loop.lo) {
	ihi = etmp->u.v;
      }
      else {
	ihi = etmp->u.v-1;
      }
      //FREE (etmp);

      vx = s->LookupVal (p->u.loop.id);
      vx->init = 1;
      vx->u.idx = s->AllocPInt();

      if (ihi < ilo) {
	/* empty */
	if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	  ret->type = ACT_PRS_EXPR_TRUE;
	}
	else {
	  ret->type = ACT_PRS_EXPR_FALSE;
	}
      }
      else {
	FREE (ret);
	ret = NULL;

	for (i=ilo; i <= ihi; i++) {
	  act_prs_expr_t *at;

	  s->setPInt (vx->u.idx, i);
	  
	  at = prs_expr_expand (p->u.loop.e, ns, s);

	  if (!ret) {
	    ret = at;
	    if (ret->type == ACT_PRS_EXPR_TRUE) {
	      if (p->type == ACT_PRS_EXPR_ANDLOOP) {
		act_free_a_prs_expr (ret);
		ret = NULL;
	      }
	      else {
		break;
	      }
	    }
	    else if (ret->type == ACT_PRS_EXPR_FALSE) {
	      if (p->type == ACT_PRS_EXPR_ANDLOOP) {
		break;
	      }
	      else {
		act_free_a_prs_expr (ret);
		ret = NULL;
	      }
	    }
	  }
	  else if (at->type == ACT_PRS_EXPR_TRUE) {
	    if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	      /* nothing */
	    }
	    else {
	      /* we're done! */
	      act_free_a_prs_expr (ret);
	      ret = at;
	      break;
	    }
	  }
	  else if (at->type == ACT_PRS_EXPR_FALSE) {
	    if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	      /* we're done */
	      act_free_a_prs_expr (ret);
	      ret = at;
	      break;
	    }
	    else {
	      /* nothing */
	    }
	  }
	  else {
	    /* ok now & and | combine! */
	    act_prs_expr_t *tmp;
	    NEW (tmp, act_prs_expr_t);
	    tmp->u.e.l = ret;
	    tmp->u.e.r = at;
	    tmp->u.e.pchg = NULL;
	    tmp->u.e.pchg_type = -1;
	    if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	      tmp->type = ACT_PRS_EXPR_AND;
	    }
	    else {
	      tmp->type = ACT_PRS_EXPR_OR;
	    }
	    ret= tmp;
	  }
	}
	if (ret == NULL) {
	  NEW (ret, act_prs_expr_t);
	  if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	    ret->type = ACT_PRS_EXPR_TRUE;
	  }
	  else {
	    ret->type = ACT_PRS_EXPR_FALSE;
	  }
	}
      }
      s->DeallocPInt (vx->u.idx, 1);
    }
    s->Del (p->u.loop.id);
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    break;
    
  default:
    Assert (0, "Eh?");
    break;
  }
  
  
  return ret;
}

static int current_attr_num;
static  char **current_attr_table;
static act_attr_t *attr_expand (act_attr_t *a, ActNamespace *ns, Scope *s)
{
  act_attr_t *hd = NULL, *tl = NULL, *tmp;
  int pos;

  while (a) {
    NEW (tmp, act_attr_t);
    tmp->attr = a->attr;

    for (pos = 0; pos < current_attr_num; pos++) {
      if (strcmp (current_attr_table[pos]+4, a->attr) == 0) break;
    }
    Assert (pos != current_attr_num, "What?");
    tmp->e = expr_expand (a->e, ns, s);

    if (current_attr_table[pos][0] == 'i' && tmp->e->type != E_INT) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Integer attribute %s is assigned a non-integer/non-constant expression\n", tmp->attr);
      fprintf (stderr, "  expr: ");
      print_expr (stderr, a->e);
      fprintf (stderr, "  evaluated to: ");
      print_expr (stderr, tmp->e);
      fprintf (stderr, "\n");
      exit (1);
    } else if (current_attr_table[pos][0] == 'r' && tmp->e->type != E_REAL) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Real attribute %s is assigned a non-real/non-constant expression\n", tmp->attr);
      fprintf (stderr, "  expr: ");
      print_expr (stderr, a->e);
      fprintf (stderr, "  evaluated to: ");
      print_expr (stderr, tmp->e);
      fprintf (stderr, "\n");
      exit (1);
    } else if (current_attr_table[pos][0] == 'b' &&
	       (tmp->e->type != E_TRUE && tmp->e->type != E_FALSE)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Boolean attribute %s is assigned a non-Boolean/non-constant expression\n", tmp->attr);
      fprintf (stderr, "  expr: ");
      print_expr (stderr, a->e);
      fprintf (stderr, "  evaluated to: ");
      print_expr (stderr, tmp->e);
      fprintf (stderr, "\n");
      exit (1);
    }
    q_ins (hd, tl, tmp);
    a = a->next;
  }
  return hd;
}


act_attr_t *prs_attr_expand (act_attr_t *a, ActNamespace *ns, Scope *s)
{
  current_attr_num = config_get_table_size ("act.prs_attr");
  current_attr_table = config_get_table_string ("act.prs_attr");
  return attr_expand (a, ns, s);
}

act_attr_t *inst_attr_expand (act_attr_t *a, ActNamespace *ns, Scope *s)
{
  current_attr_num = config_get_table_size ("act.instance_attr");
  current_attr_table = config_get_table_string ("act.instance_attr");
  return attr_expand (a, ns, s);
}

act_size_spec_t *act_expand_size (act_size_spec_t *sz, ActNamespace *ns, Scope *s)
{
  act_size_spec_t *ret;
  if (!sz) return NULL;

  NEW (ret, act_size_spec_t);
  if (sz->w) {
    ret->w = expr_expand (sz->w, ns, s);
    if (ret->w->type != E_INT && ret->w->type != E_REAL) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Size expression is not of type pint or preal\n");
      fprintf (stderr, " expr: ");
      print_expr (stderr, sz->w);
      fprintf (stderr, "\n");
      exit (1);
   }
  }
  else {
    ret->w = NULL;
  }
  if (sz->l) {
    ret->l = expr_expand (sz->l, ns, s);
    if (ret->l->type != E_INT && ret->l->type != E_REAL) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Size expression is not of type pint or preal\n");
      fprintf (stderr, " expr: ");
      print_expr (stderr, sz->l);
      fprintf (stderr, "\n");
      exit (1);
    }
  }
  else {
    ret->l = NULL;
  }
  if (sz->folds) {
    ret->folds = expr_expand (sz->folds, ns, s);
    if (ret->folds->type != E_INT) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Size folding amount is not a pint\n");
      fprintf (stderr, " expr: ");
      print_expr (stderr, sz->folds);
      exit (1);
    }
  }
  else {
    ret->folds = NULL;
  }
  ret->flavor = sz->flavor;
  
  return ret;
}

struct act_prsmerge {
  act_prs_lang_t *vup;		/* previous prs */
  act_prs_lang_t *vdn;		/* previous prs */
};
  
  
act_prs_lang_t *prs_expand (act_prs_lang_t *p, ActNamespace *ns, Scope *s)
{
  act_prs_lang_t *hd = NULL;
  act_prs_lang_t *tl = NULL;
  act_prs_lang_t *tmp;
  ActId *idtmp;
  Expr *etmp;

  while (p) {
    /* expand one prs */
    NEW (tmp, act_prs_lang_t);
    tmp->type = p->type;
    tmp->next = NULL;
    switch (p->type) {
    case ACT_PRS_RULE:
      tmp->u.one.attr = prs_attr_expand (p->u.one.attr, ns, s);
      tmp->u.one.e = prs_expr_expand (p->u.one.e, ns, s);
      tmp->u.one.eopp = p->u.one.eopp ?
	prs_expr_expand (p->u.one.eopp, ns, s) : NULL;

      if (p->u.one.label == 0) {
	act_connection *ac;
	
	idtmp = p->u.one.id->Expand (ns, s);
	etmp = idtmp->Eval (ns, s);
	Assert (etmp->type == E_VAR, "Hmm");
	tmp->u.one.id = (ActId *)etmp->u.e.l;
	FREE (etmp);

	ac = idtmp->Canonical (s);
	if (ac->getDir() == Type::IN) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "\t");
	  idtmp->Print (stderr);
	  fprintf (stderr, " has a directional type that is not writable.\n");
	  fatal_error ("A `bool?' cannot be on the RHS of a production rule.");
	}
      }
      else {
	/* it is a char* */
	tmp->u.one.id = p->u.one.id;
	if (p->u.one.arrow_type != 0) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "@-expressions must only use -> arrows");
	  exit (1);
	}
      }
      tmp->u.one.arrow_type = p->u.one.arrow_type;
      tmp->u.one.dir = p->u.one.dir;
      tmp->u.one.label = p->u.one.label;
      break;
      
    case ACT_PRS_GATE:
      tmp->u.p.attr = prs_attr_expand (p->u.p.attr, ns, s);
      idtmp = p->u.p.s->Expand (ns, s);
      etmp = idtmp->Eval (ns, s);
      Assert (etmp->type == E_VAR, "Hm");
      tmp->u.p.s = (ActId *)etmp->u.e.l;
      FREE (etmp);

      idtmp = p->u.p.d->Expand (ns, s);
      etmp = idtmp->Eval (ns, s);
      Assert (etmp->type == E_VAR, "Hm");
      tmp->u.p.d = (ActId *)etmp->u.e.l;
      FREE (etmp);

      if (p->u.p.g) {
	idtmp = p->u.p.g->Expand (ns, s);
	etmp = idtmp->Eval (ns, s);
	Assert (etmp->type == E_VAR, "Hm");
	tmp->u.p.g = (ActId *)etmp->u.e.l;
	FREE (etmp);
      }
      else {
	tmp->u.p.g = NULL;
      }

      if (p->u.p._g) {
	idtmp = p->u.p._g->Expand (ns, s);
	etmp = idtmp->Eval (ns, s);
	Assert (etmp->type == E_VAR, "Hm");
	tmp->u.p._g = (ActId *)etmp->u.e.l;
	FREE (etmp);
      }
      else {
	tmp->u.p._g = NULL;
      }
      tmp->u.p.sz = act_expand_size (p->u.p.sz, ns, s);
      break;
      
    case ACT_PRS_LOOP:
      Assert (s->Add (p->u.l.id, TypeFactory::Factory()->NewPInt()),
	      "Should have been caught earlier");
      {
	ValueIdx *vx;
	int ilo, ihi, i;
	act_prs_lang_t *px, *pxrethd, *pxrettl;
	Expr *etmp;
	
	if (p->u.l.lo) {
	  etmp = expr_expand (p->u.l.lo, ns, s);
	  if (etmp->type != E_INT) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, "Expanding loop range in prs body\n  expr: ");
	    print_expr (stderr, p->u.l.lo);
	    fprintf (stderr, "\nNot a constant int\n");
	    exit (1);
	  }
	  ilo = etmp->u.v;
	  //FREE (etmp);
	}
	else {
	  ilo = 0;
	}
	etmp = expr_expand (p->u.l.hi, ns, s);
	if (etmp->type != E_INT) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "Expanding loop range in prs body\n  expr: ");
	  print_expr (stderr, p->u.l.hi);
	  fprintf (stderr, "\nNot a constant int\n");
	  exit (1);
	}
	if (p->u.l.lo) {
	  ihi = etmp->u.v;
	}
	else {
	  ihi = etmp->u.v-1;
	}
	//FREE (etmp);

	vx = s->LookupVal (p->u.l.id);
	vx->init = 1;
	vx->u.idx = s->AllocPInt();

	pxrethd = NULL;
	pxrettl = NULL;
	for (i = ilo; i <= ihi; i++) {
	  s->setPInt (vx->u.idx, i);
	  px = prs_expand (p->u.l.p, ns, s);
	  if (!pxrethd) {
	    pxrethd = px;
	    pxrettl = pxrethd;
	    while (pxrettl->next) {
	      pxrettl = pxrettl->next;
	    }
	  }
	  else {
	    pxrettl->next = px;
	    while (pxrettl->next) {
	      pxrettl = pxrettl->next;
	    }
	  }
	}
	FREE (tmp);
	tmp = pxrethd;
	s->DeallocPInt (vx->u.idx, 1);
      }
      s->Del (p->u.l.id);
      break;
      
    case ACT_PRS_TREE:
      if (p->u.l.lo) {
	etmp = expr_expand (p->u.l.lo, ns, s);
	if (etmp->type != E_INT) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "tree<> expression is not of type pint\n");
	  fprintf (stderr, " expr: ");
	  print_expr (stderr, p->u.l.lo);
	  fprintf (stderr, "\n");
	  exit (1);
	}
      }
      else {
	etmp = NULL;
      }
      tmp->u.l.lo = etmp;
      tmp->u.l.hi = NULL;
      tmp->u.l.id = NULL;
      tmp->u.l.p = prs_expand (p->u.l.p, ns, s);
      break;
      
    case ACT_PRS_SUBCKT:
      tmp->u.l.id = p->u.l.id;
      tmp->u.l.lo = NULL;
      tmp->u.l.hi = NULL;
      tmp->u.l.p = prs_expand (p->u.l.p, ns, s);
      break;
      
    default:
      Assert (0, "Should not be here");
      break;
    }
    if (tmp) {
      if (!hd) {
	hd = tmp;
	tl = tmp;
      }
      else {
	tl->next = tmp;
      }
      while (tl->next) {
	tl = tl->next;
      }
    }
    p = q_step (p);
  }
  return hd;
}

act_chp *chp_expand (act_chp *c, ActNamespace *ns, Scope *s)
{
  act_chp *ret;
  if (!c) return NULL;

  NEW (ret, act_chp);
  
  ret->vdd = fullexpand_var (c->vdd, ns, s);
  ret->gnd = fullexpand_var (c->gnd, ns, s);
  ret->psc = fullexpand_var (c->psc, ns, s);
  ret->nsc = fullexpand_var (c->nsc, ns, s);
  ret->c = chp_expand (c->c, ns, s);
  
  return ret;
}

act_chp_lang_t *chp_expand (act_chp_lang_t *c, ActNamespace *ns, Scope *s)
{
  act_chp_lang_t *ret;
  act_chp_gc_t *gchd, *gctl, *gctmp, *tmp;
  listitem_t *li;
  ValueIdx *vx;
  
  if (!c) return NULL;
  NEW (ret, act_chp_lang_t);
  ret->type = c->type;
  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    ret->u.semi_comma.cmd = list_new ();
    for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
      list_append (ret->u.semi_comma.cmd, chp_expand ((act_chp_lang_t *)list_value (li), ns, s));
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    gchd = NULL;
    gctl = NULL;
    for (gctmp = c->u.gc; gctmp; gctmp = gctmp->next) {
      if (gctmp->id) {
	int ilo, ihi;
	Expr *ix;
	Assert (s->Add (gctmp->id, TypeFactory::Factory()->NewPInt()),
		"What?");
	vx = s->LookupVal (gctmp->id);
	vx->init = 1;
	vx->u.idx = s->AllocPInt();
	ix = expr_expand (gctmp->lo, ns, s);
	if (!expr_is_a_const (ix)) {
	  act_error_ctxt (stderr);
	  print_expr (stderr, gctmp->lo);
	  fprintf (stderr, "\n");
	  fatal_error ("Isn't a constant expression");
	}
	ilo = ix->u.v;
	if (gctmp->hi) {
	  ix = expr_expand (gctmp->lo, ns, s);
	  if (!expr_is_a_const (ix)) {
	    act_error_ctxt (stderr);
	    print_expr (stderr, gctmp->lo);
	    fprintf (stderr, "\n");
	    fatal_error ("Isn't a constant expression");
	  }
	  ihi = ix->u.v;
	}
	else {
	  ihi = ilo-1;
	  ilo = 0;
	}
	for (int iter=ilo; iter <= ihi; iter++) {
	  s->setPInt (vx->u.idx, iter);
	  NEW (tmp, act_chp_gc_t);
	  tmp->next = NULL;
	  tmp->id = NULL;
	  tmp->g = expr_expand (gctmp->g, ns, s);
	  tmp->s = chp_expand (gctmp->s, ns, s);
	  q_ins (gchd, gctl, tmp);
	}
	s->DeallocPInt (vx->u.idx, 1);
	s->Del (gctmp->id);
      }
      else {	
	NEW (tmp, act_chp_gc_t);
	tmp->id = NULL;
	tmp->next = NULL;
	tmp->g = expr_expand (gctmp->g, ns, s);
	tmp->s = chp_expand (gctmp->s, ns, s);
	q_ins (gchd, gctl, tmp);
      }
    }
    ret->u.gc = gchd;
    break;

  case ACT_CHP_SKIP:
    break;

  case ACT_CHP_ASSIGN:
    ret->u.assign.id = expand_var_write (c->u.assign.id, ns, s);
    ret->u.assign.e = expr_expand (c->u.assign.e, ns, s);
    break;
    
  case ACT_CHP_SEND:
    ret->u.comm.chan = expand_var_chan (c->u.comm.chan, ns, s);
    ret->u.comm.rhs = list_new ();
    for (li = list_first (c->u.comm.rhs); li; li = list_next (li)) {
      list_append (ret->u.comm.rhs,
		   expr_expand ((Expr *)list_value (li), ns, s));
    }
    break;
    
  case ACT_CHP_RECV:
    ret->u.comm.chan = expand_var_chan (c->u.comm.chan, ns, s);
    ret->u.comm.rhs = list_new ();
    for (li = list_first (c->u.comm.rhs); li; li = list_next (li)) {
      list_append (ret->u.comm.rhs,
		   expand_var_write ((ActId *)list_value (li), ns, s));
    }
    break;

  case ACT_CHP_FUNC:
    ret->u.func.name = c->u.func.name;
    ret->u.func.rhs = list_new ();
    for (li = list_first (c->u.func.rhs); li; li = list_next (li)) {
      act_func_arguments_t *arg, *ra;
      NEW (arg, act_func_arguments_t);
      ra = (act_func_arguments_t *) list_value (li);
      arg->isstring = ra->isstring;
      if (ra->isstring) {
	arg->u.s = ra->u.s;
      }
      else {
	arg->u.e = expr_expand (ra->u.e, ns, s);
      }
      list_append (ret->u.func.rhs, arg);
    }
    break;
    
  default:
    break;
  }
  return ret;
}


act_spec *spec_expand (act_spec *s, ActNamespace *ns, Scope *sc)
{
  act_spec *ret = NULL;
  act_spec *prev = NULL;
  act_spec *tmp = NULL;

  while (s) {
    NEW (tmp, act_spec);
    tmp->type = s->type;
    tmp->count = s->count;
    tmp->isrequires = s->isrequires;
    MALLOC (tmp->ids, ActId *, tmp->count);
    for (int i=0; i < tmp->count-1; i++) {
      tmp->ids[i] = s->ids[i]->Expand (ns, sc);
    }
    if (tmp->type != -1) {
      tmp->ids[tmp->count-1] = s->ids[tmp->count-1]->Expand (ns, sc);
    }
    else {
      tmp->ids[tmp->count-1] = (ActId *) (s->ids[tmp->count-1] ?
				expr_expand ((Expr *)s->ids[tmp->count-1],
					     ns, sc) : NULL);
    }
    tmp->extra = s->extra;
    tmp->next = NULL;
    if (prev) {
      prev->next = tmp;
    }
    else {
      ret = tmp;
    }
    prev = tmp;
    s = s->next;
  }
  return ret;
}

const char *act_spec_string (int type)
{
  static int num = -1;
  static char **opts;

  if (num == -1) {
    num = config_get_table_size ("act.spec_types");
    opts = config_get_table_string ("act.spec_types");
  }

  if (type == -1) {
    return "timing";
  }

  if (type < 0 || type >= num) {
    return NULL;
  }
  return opts[type];
}

static void _print_attr (FILE *fp, act_attr_t *a)
{
  fprintf (fp, "[");
  while (a) {
    fprintf (fp, "%s=", a->attr);
    print_expr (fp, a->e);
    if (a->next) {
      fprintf (fp, "; ");
    }
    a = a->next;
  }
  fprintf (fp, "]");
}

static void _print_size (FILE *fp, act_size_spec_t *sz)
{
  if (sz) {
    fprintf (fp, "<");
    if (sz->w) {
      print_expr (fp, sz->w);
      if (sz->l) {
	fprintf (fp, ",");
	print_expr (fp, sz->l);
      }
      if (sz->flavor != 0) {
	fprintf (fp, ",%s", act_dev_value_to_string (sz->flavor));
      }
    }
    if (sz->folds) {
      fprintf (fp, ";");
      print_expr (fp, sz->folds);
    }
    fprintf (fp, ">");
  }
}

void act_print_size (FILE *fp, act_size_spec_t *sz)
{
  _print_size (fp, sz);
}

static void _print_prs_expr (FILE *fp, act_prs_expr_t *e, int prec)
{
  if (!e) return;
 
#define PREC_BEGIN(myprec)			\
  do {						\
    if ((myprec) < prec) {			\
      fprintf (fp, "(");			\
    }						\
  } while (0)

#define PREC_END(myprec)			\
  do {						\
    if ((myprec) < prec) {			\
      fprintf (fp, ")");			\
    }						\
  } while (0)

#define EMIT_BIN(myprec,sym)					\
  do {								\
    PREC_BEGIN(myprec);						\
    _print_prs_expr (fp, e->u.e.l, (myprec));			\
    fprintf (fp, "%s", (sym));					\
    if (e->u.e.pchg) {						\
      fprintf (fp, "{%c", e->u.e.pchg_type ? '+' : '-');	\
      _print_prs_expr (fp, e->u.e.pchg, 0);				\
      fprintf (fp, "}");					\
    }								\
    _print_prs_expr (fp, e->u.e.r, (myprec));			\
    PREC_END (myprec);						\
  } while (0)

#define EMIT_UNOP(myprec,sym)			\
  do {						\
    PREC_BEGIN(myprec);				\
    fprintf (fp, "%s", sym);			\
    _print_prs_expr (fp, e->u.e.l, (myprec));	\
    PREC_END (myprec);				\
  } while (0)
    
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
    EMIT_BIN(3,"&");
    break;
  case ACT_PRS_EXPR_OR:
    EMIT_BIN(2,"|");
    break;
  case ACT_PRS_EXPR_NOT:
    EMIT_UNOP(4, "~");
    break;

  case ACT_PRS_EXPR_TRUE:
    fprintf (fp, "true");
    break;

  case ACT_PRS_EXPR_FALSE:
    fprintf (fp, "false");
    break;

  case ACT_PRS_EXPR_LABEL:
    fprintf (fp, "@%s", e->u.l.label);
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    fprintf (fp, "(%c%s:", e->type == ACT_PRS_EXPR_ANDLOOP ? '&' : '|',
	     e->u.loop.id);
    print_expr (fp, e->u.loop.lo);
    if (e->u.loop.hi) {
      fprintf (fp, " .. ");
      print_expr (fp, e->u.loop.hi);
    }
    fprintf (fp, ":");
    _print_prs_expr (fp, e->u.loop.e, 0);
    break;

  case ACT_PRS_EXPR_VAR:
    e->u.v.id->Print (fp);
    if (e->u.v.sz) {
      _print_size (fp, e->u.v.sz);
    }
    break;
  default:
    fatal_error ("What?");
    break;
  }
}

static void _print_one_prs (FILE *fp, act_prs_lang_t *prs)
{
  if (!prs) return;
  switch (prs->type) {
  case ACT_PRS_RULE:
    if (prs->u.one.attr) {
      _print_attr (fp, prs->u.one.attr);
    }
    _print_prs_expr (fp, prs->u.one.e, 0);
    if (prs->u.one.arrow_type == 0) {
      fprintf (fp, " -> ");
    }
    else if (prs->u.one.arrow_type == 1) {
      fprintf (fp, " => ");
    }
    else if (prs->u.one.arrow_type == 2) {
      fprintf (fp, " #> ");
    }
    else {
      fatal_error ("Eh?");
    }
    if (prs->u.one.label) {
      fprintf (fp, "@%s", (char *)prs->u.one.id);
    }
    else {
      prs->u.one.id->Print (fp);
    }
    if (prs->u.one.dir) {
      fprintf (fp, "+\n");
    }
    else {
      fprintf (fp, "-\n");
    }
    break;
  case ACT_PRS_GATE:
    if (prs->u.p.attr) {
      _print_attr (fp, prs->u.p.attr);
    }
    if (prs->u.p.g && prs->u.p._g) {
      fprintf (fp, "transgate");
    }
    else if (prs->u.p.g) {
      fprintf (fp, "passn");
    }
    else if (prs->u.p._g) {
      fprintf (fp, "passp");
    }
    else {
      Assert (0, "Hmm");
    }
    if (prs->u.p.sz) {
      _print_size (fp, prs->u.p.sz);
    }
    fprintf (fp, "(");
    if (prs->u.p.g) {
      prs->u.p.g->Print (fp);
      fprintf (fp, ",");
    }
    if (prs->u.p._g) {
      prs->u.p._g->Print (fp);
      fprintf (fp, ",");
    }
    prs->u.p.s->Print (fp);
    fprintf (fp, ",");
    prs->u.p.d->Print (fp);
    fprintf (fp, ")\n");
    break;
  case ACT_PRS_LOOP:
    fprintf (fp, "(%s:", prs->u.l.id);
    if (prs->u.l.lo) {
      print_expr (fp, prs->u.l.lo);
      fprintf (fp, " .. ");
    }
    print_expr (fp, prs->u.l.hi);
    fprintf (fp, ":");
    for (act_prs_lang_t *i = prs->u.l.p; i; i = i->next) {
      _print_one_prs (fp, i);
    }
    fprintf (fp, ")\n");
    break;
  case ACT_PRS_TREE:
    fprintf (fp, "tree");
    if (prs->u.l.lo) {
      fprintf (fp, "<");
      print_expr (fp, prs->u.l.lo);
      fprintf (fp, ">");
    }
    fprintf (fp, "{\n");
    for (act_prs_lang_t *i = prs->u.l.p; i; i = i->next) {
      _print_one_prs (fp, i);
    }
    fprintf (fp, "}\n");
    break;
  case ACT_PRS_SUBCKT:
    fprintf (fp, "subckt");
    if (prs->u.l.id) {
      fprintf (fp, "<%s>", prs->u.l.id);
    }
    fprintf (fp, "{\n");
    for (act_prs_lang_t *i = prs->u.l.p; i; i = i->next) {
      _print_one_prs (fp, i);
    }
    fprintf (fp, "}\n");
    break;
  default:
    fatal_error ("Unsupported");
    break;
  }
}

void prs_print (FILE *fp, act_prs *prs)
{
  while (prs) {
    fprintf (fp, "prs ");
    if (prs->vdd) {
      fprintf (fp, "<");
      prs->vdd->Print (fp);
      if (prs->gnd) {
	fprintf (fp, ", ");
	prs->gnd->Print (fp);
      }
      if (prs->psc) {
	fprintf (fp, " | ");
	prs->psc->Print (fp);
	fprintf (fp, ", ");
	prs->nsc->Print (fp);
      }
      fprintf (fp, "> ");
    }
    fprintf (fp, "{\n");
    act_prs_lang_t *p;
    for (p = prs->p; p; p = p->next) {
      _print_one_prs (fp, p);
    }
    fprintf (fp, "}\n");
    prs = prs->next;
  }
}

void chp_print (FILE *fp, act_chp_lang_t *c)
{
  if (!c) return;
  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    {
      listitem_t *li;
      for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li)) {
	chp_print (fp, (act_chp_lang_t *)list_value (li));
	if (list_next (li)) {
	  if (c->type == ACT_CHP_COMMA) {
	    fprintf (fp, ",");
	  }
	  else {
	    fprintf (fp, ";");
	  }
	}
      }
    }
    break;

  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    fprintf (fp, "*");
  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
    if (c->type == ACT_CHP_SELECT_NONDET) {
      fprintf (fp, "|");
    }
    fprintf (fp, "[");
    {
      act_chp_gc_t *gc = c->u.gc;

      if (c->type == ACT_CHP_DOLOOP) {
	fprintf (fp, " ");
	chp_print (fp, gc->s);
	fprintf (fp, " <- ");
	if (gc->g) {
	  print_expr (fp, gc->g);
	}
	else {
	  fprintf (fp, "true");
	}
      }
      else {
	while (gc) {
	  if (!gc->g) {
	    if (c->type == ACT_CHP_LOOP) {
	      fprintf (fp, "true");
	    }
	    else {
	      fprintf (fp, "else");
	    }
	  }
	  else {
	    print_expr (fp, gc->g);
	  }
	  if (gc->s) {
	    fprintf (fp, " -> ");
	    chp_print (fp, gc->s);
	  }
	  if (gc->next) {
	    fprintf (fp, " [] ");
	  }
	  gc = gc->next;
	}
      }
    }
    fprintf (fp, "]");
    if (c->type == ACT_CHP_SELECT_NONDET) {
      fprintf (fp, "|");
    }
    break;
    
  case ACT_CHP_SKIP:
    fprintf (fp, "skip");
    break;

  case ACT_CHP_ASSIGN:
    c->u.assign.id->Print (fp);
    fprintf (fp, ":=");
    print_expr (fp, c->u.assign.e);
    break;
    
  case ACT_CHP_SEND:
    c->u.comm.chan->Print (fp);
    fprintf (fp, "!");
    if (list_length (c->u.comm.rhs) > 1) {
      fprintf (fp, "(");
    }
    {
      listitem_t *li;
      for (li = list_first (c->u.comm.rhs); li; li = list_next (li)) {
	print_expr (fp, (Expr *)list_value (li));
	if (list_next (li)) {
	  fprintf (fp, ",");
	}
      }
    }
    if (list_length (c->u.comm.rhs) > 1) {
      fprintf (fp, ")");
    }
    break;
    
  case ACT_CHP_RECV:
    c->u.comm.chan->Print (fp);
    fprintf (fp, "?");
    if (list_length (c->u.comm.rhs) > 1) {
      fprintf (fp, "(");
    }
    {
      listitem_t *li;
      for (li = list_first (c->u.comm.rhs); li; li = list_next (li)) {
	((ActId *)list_value (li))->Print (fp);
	if (list_next (li)) {
	  fprintf (fp, ",");
	}
      }
    }
    if (list_length (c->u.comm.rhs) > 1) {
      fprintf (fp, ")");
    }
    break;

  case ACT_CHP_FUNC:
  default:
    fatal_error ("Unknown type");
    break;
  }
}

void chp_print (FILE *fp, act_chp *chp)
{
  fprintf (fp, "chp {\n");
  chp_print (fp, chp->c);
  fprintf (fp, "\n}\n");
}

void hse_print (FILE *fp, act_chp *chp)
{
  fprintf (fp, "hse {\n");
  chp_print (fp, chp->c);
  fprintf (fp, "\n}\n");
}


void spec_print (FILE *fp, act_spec *spec)
{
  int count = config_get_table_size ("act.spec_types");
  char **specs = config_get_table_string ("act.spec_types");
  fprintf (fp, "spec {\n");
  while (spec) {
    if (spec->type == -1) {
      fprintf (fp, "   timing ");

#define SPEC_PRINT_ID(x)			\
      do {					\
	if (spec->extra[x] & 0x04) {		\
	  fprintf (fp, "?");			\
	}					\
	spec->ids[x]->Print (fp);		\
	if (spec->extra[x] & 0x8) {		\
	  fprintf (fp, "*");			\
	}					\
	if (spec->extra[x] & 0x3) {		\
	  if ((spec->extra[x] & 0x03) == 1) {	\
	    fprintf (fp, "+");			\
	  }					\
	  else {				\
	    fprintf (fp, "-");			\
	  }					\
	}					\
      } while (0)

      if (spec->ids[0]) {
	SPEC_PRINT_ID(0);
	fprintf (fp, " : ");
      }
      SPEC_PRINT_ID (1);
      fprintf (fp, " < ");
      if (spec->ids[3]) {
	fprintf (fp, "[");
	print_expr (fp, (Expr *)spec->ids[3]);
	fprintf (fp, "] ");
      }
      SPEC_PRINT_ID(2);
      fprintf (fp, "\n");
    }
    else {
      Assert (spec->type >= 0 && spec->type < count, "What?");
      fprintf (fp, "   %s(", specs[spec->type]);
      for (int i=0; i < spec->count; i++) {
	if (i != 0) {
	  fprintf (fp, ", ");
	}
	spec->ids[i]->Print (fp);
      }
      fprintf (fp, ")\n");
    }
    spec = spec->next;
  }
  fprintf (fp, "}\n");
}

void refine_expand (act_refine *r, ActNamespace *ns, Scope *s)
{
  if (!r) return;
  if (r->b) {
    r->b->Expandlist (ns, s);
  }
}

act_languages *act_languages::Expand (ActNamespace *ns, Scope *s)
{
  act_languages *ret = new act_languages ();
  if (chp) {
    ret->chp = chp_expand (chp, ns, s);
  }
  if (hse) {
    ret->hse = chp_expand (hse, ns, s);
  }
  if (prs) {
    ret->prs = prs_expand (prs, ns, s);
  }
  if (refine) {
    refine_expand (refine, ns, s);
  }
  return ret;
}


void refine_print (FILE *fp, act_refine *r)
{
  if (!r || !r->b) return;
  r->b->Print (fp);
}
