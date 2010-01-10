#!/usr/bin/env bash
if [ ! $# -eq 1 ]; then
  exec >&2
  echo "Usage: $0 <language>"
  echo "  language: two-letter language code + country code if applicable (de, en_GB)"
  exit 1
fi

TS2PO=`type -p ts2po`
LCONV=lconvert
POT=quassel.pot
BASE=$1
PO=$BASE.po
TS=$BASE.ts

( [ -f $PO ] || ( [ -f $POT ] && cp $POT $PO ) )                      &&
  $LCONV -i $PO -o $TS                                                &&
  lupdate -no-obsolete ../src -ts $TS                                 &&
  ${TS2PO:-$LCONV} -i $TS -o tmp.${PO} ${TS2PO:+--duplicates=msgctxt} &&
  msguniq --use-first tmp.${PO} > ${PO}                               &&
  rm $TS tmp.${PO}

[ $? -ne 0 ] && echo "Something went wrong"
