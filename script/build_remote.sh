#!/bin/#!/usr/bin/env bash

ssh -t fat 'cd /data/zhenyus/webcachesim/build; git pull; git checkout log1p_tuning; git pull; make -j32;'


#ssh -t clnode183.clemson.cloudlab.us 'cd /proj/cops-PG0/workspaces/zhenyus/webcachesim/build; git pull; git checkout log1p; git pull; make -j32;'