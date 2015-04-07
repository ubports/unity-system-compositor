# Copyright © 2015 Canonical Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>

header=$1
varname=$2
filename=$3

header_guard=$(echo "USC_$(basename $header)_" | tr '[a-z].' '[A-Z]_')

echo "#ifndef $header_guard
#define $header_guard
const char* const $varname = R\"($(cat $filename))\";
#endif" > $header
