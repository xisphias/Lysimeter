scale1hr<-data.frame(dat=read.table("/Users/jac/Documents/projects/plantWater/lysim/Lysimeter/node/serialcaptures/CoolTerm Capture 2017-06-18 15-28-07.txt")[,1])
plot(scale1hr$dat*1000,type='l')
scale1hr$diff<-scale1hr$dat-mean(scale1hr$dat[1:10])
plot(scale1hr$diff*1000,type='l')
