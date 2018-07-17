/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <act/iter.h>

ActNamespaceiter::ActNamespaceiter (ActNamespace *ns)
{
  i = 0;
  b = NULL;
  top = ns;
}


ActNamespaceiter::ActNamespaceiter (const ActNamespaceiter &c)
{
  i = c.i;
  b = c.b;
  top = c.top;
}
  
ActNamespaceiter ActNamespaceiter::operator++(int)
{
  ActNamespaceiter tmp(*this);
  operator++();
  return tmp;
}

bool ActNamespaceiter::operator==(const ActNamespaceiter& rhs) const
{
 return (rhs.top == top) && (rhs.i == i) && (rhs.b == b);
}


bool ActNamespaceiter::operator!=(const ActNamespaceiter& rhs) const
{
 return !operator==(rhs);
}

ActNamespace *ActNamespaceiter::operator*()
{
 if (b) {
   return (ActNamespace *)b->v;
 }
 else {
   return NULL;
 }
}

ActNamespaceiter ActNamespaceiter::begin()
{
  ActNamespaceiter iter(*this);
  iter.i = 0;
  iter.b = NULL;
  iter++;
  return iter;
}

ActNamespaceiter ActNamespaceiter::end()
{
  ActNamespaceiter iter(*this);
  iter.b = NULL;
  iter.i = top->N->size;
  return iter;
}

ActNamespaceiter& ActNamespaceiter::operator++()
{
  /* initial: i=0, b is NULL */
  if (b && b->next) {
    b = b->next;
  }
  else {
    if (b) {
      i++;
    }
    b = NULL;
    while (!top->N->head[i] && i < top->N->size) {
      i++;
    }
    if (i != top->N->size) {
      b = top->N->head[i];
    }
  }
  return *this;
}


/*------------------------------------------------------------------------*/

ActInstiter::ActInstiter (Scope *_s)
{
  i = 0;
  b = NULL;
  s = _s;
  Assert (s->isExpanded(), "Only for expanded scopes");
}


ActInstiter::ActInstiter (const ActInstiter &c)
{
  i = c.i;
  b = c.b;
  s = c.s;
}
  
ActInstiter ActInstiter::operator++(int)
{
  ActInstiter tmp(*this);
  operator++();
  return tmp;
}

bool ActInstiter::operator==(const ActInstiter& rhs) const
{
 return (rhs.s == s) && (rhs.i == i) && (rhs.b == b);
}


bool ActInstiter::operator!=(const ActInstiter& rhs) const
{
 return !operator==(rhs);
}

ValueIdx *ActInstiter::operator*()
{
 if (b) {
   return (ValueIdx *)b->v;
 }
 else {
   return NULL;
 }
}

ActInstiter ActInstiter::begin()
{
  ActInstiter iter(*this);
  iter.i = 0;
  iter.b = NULL;
  iter++;
  return iter;
}

ActInstiter ActInstiter::end()
{
  ActInstiter iter(*this);
  iter.b = NULL;
  iter.i = s->H->size;
  return iter;
}

ActInstiter& ActInstiter::operator++()
{
  /* initial: i=0, b is NULL */
  if (b && b->next) {
    b = b->next;
  }
  else {
    if (b) {
      i++;
    }
    b = NULL;
    while (!s->H->head[i] && i < s->H->size) {
      i++;
    }
    if (i != s->H->size) {
      b = s->H->head[i];
    }
  }
  return *this;
}
