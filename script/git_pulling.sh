#!/bin/bash

PYTHON='/home/zhenyus/anaconda3/envs/webcachesim_env/bin/python'
RUNNER_FILE='/home/zhenyus/webcachesim/script/run.py'

while [[ 1 ]]; do
  git remote update
  UPSTREAM=${1:-'@{u}'}
  LOCAL=$(git rev-parse @)
  REMOTE=$(git rev-parse "$UPSTREAM")
  BASE=$(git merge-base @ "$UPSTREAM")

  if [ $LOCAL = $REMOTE ]; then
      echo "Up-to-date"
  elif [ $LOCAL = $BASE ]; then
      echo "Need to pull"
      echo "pulling"
      git pull
      $PYTHON $RUNNER_FILE
  elif [ $REMOTE = $BASE ]; then
      echo "Need to push"
  else
      echo "Diverged"
  fi
  sleep 30
done