# anscheduler

The purpose of this project is to provide an abstract task scheduler that is testable and can be integrated into operating systems.

# Subtree

The `anidxset` subtree was setup as follows:

    git remote add anidxset https://github.com/unixpickle/anidxset.git
    git fetch anidxset
    git checkout -b anpages anidxset/master
    git checkout master
    git read-tree --prefix=lib/anidxset/ -u anidxset

To pull upstream changes from `anidxset`, use:

    git checkout anidxset
    git pull
    git checkout master
    git merge --squash -s subtree --no-commit anidxset
