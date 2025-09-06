#!/bin/sh
sed -i'' 's/<div id="Middle" class=.clear.>/<div class="Middle clear">/' new_index.html
sed -i'' 's/<div id="Middle">/<div class="Middle">/' new_index.html
sed -i'' 's/<div id="Content" class="partialWidth">/<div class="Content partialWidth">/' new_index.html
sed -i'' 's/<div id="Content">/<div class="Content">/' new_index.html
sed -i'' 's/href="news.html">/href="#News">/' new_index.html
sed -i'' 's/href="installation.html">/href="#Installation">/' new_index.html
sed -i'' 's/href="guide.html">/href="#Guide">/' new_index.html
