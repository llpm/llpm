#!/bin/bash

for i in obj/*_elab*.gv;
do
    echo Running dot for $i
    dot -Tpdf -o ${i}.pdf $i || true
done

pdfunite obj/*_elab*.gv.pdf elab.pdf

for i in obj/*_opt*.gv;
do
    echo Running dot for $i
    dot -Tpdf -o ${i}.pdf $i || true
done

pdfunite obj/*_opt*.gv.pdf opt.pdf
