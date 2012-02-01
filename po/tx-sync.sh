#!/usr/bin/env bash
pushd /usr/src/quassel && ( 
  currb=$(git name-rev --name-only HEAD)
  git checkout -q i18n-tx-sync && (
    EDITOR=/bin/true /usr/src/quassel/po/pull-from-transifex.sh -f &&
      git push -q
  ); git checkout -q "$currb"
); popd
