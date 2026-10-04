# Make the upstream Setup25 header visible to every translation unit receiving
# the forced include. This preserves upstream pins without editing library files.
Import("env")
from os.path import join
env.Append(CPPPATH=[join(env.subst("$PROJECT_LIBDEPS_DIR"), env.subst("$PIOENV"), "TFT_eSPI")])
