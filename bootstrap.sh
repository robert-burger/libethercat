#!/bin/bash

safe_branch=$(git describe --tags | tr '/:' '--')

sed "s|PACKAGE_VERSION|$safe_branch|" configure.ac.in > configure.ac
