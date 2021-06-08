for bump in *bump.png
do
    normal=$(echo $bump | sed -e "s/bump/normal/g")
    normal=$(echo $normal | sed -e "s/png/jpg/g")
    /home/savas/Projects/normal_the_bumps/normal_the_bumps $bump $normal
done
