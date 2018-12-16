#!/bin/bash
cat ${*} | egrep --only-match '^(HIDDEN )?\w.* ..*\(..*\)'
