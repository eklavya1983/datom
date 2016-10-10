#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

function loginfo {
    printf "${GREEN}$@${NC}\n"
}

function logwarn {
    printf "${RED}$@${NC}\n"
}
