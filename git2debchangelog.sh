#!/bin/bash
#
# Convert git log into changelog in Debian format
#
# Tags in format 1.2.3-4 become version entries. Log entries between them
# become changelog entries. Merge commits are not excluded, so you probably
# have to clean up the result manually.

RE_VERSION='^v\?[0-9]\+\([.-][0-9]\+\)*'
# Assume the name of the current directory is the package name
PACKAGE=${PWD##*/}

function logentry() {
        local previous=$1
        local version=$2
        echo "$PACKAGE ($version) unstable; urgency=low"
        echo
        git --no-pager log --format="  * %s" $previous${previous:+..}$version
        echo
        git --no-pager log --format=" -- %an <%ae>  %aD" -n 1 $version
        echo
}

git tag --sort "-version:refname" | grep -v beta | grep -v rev | grep "$RE_VERSION" | (
        read version; 

        logentry $version master
        while read previous; do
                logentry $previous $version
                version="$previous"
        done
        logentry "" $version
)
