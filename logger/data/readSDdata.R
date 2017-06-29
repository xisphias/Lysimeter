as.POSIXct(1497951052, origin="1970-01-01",tz="UTC")
as.POSIXct(1497980385, origin="1970-01-01",tz="UTC")
library(ggplot2)
# as.POSIXct(1496679283, origin="1970-01-01",tz="UTC")
# dat<-read.csv("test_20170601_100_9.csv")
# dat$DateTime<-as.POSIXct(dat$unixtime+(8*60*60), origin="1970-01-01") #corrects for setting rtc to akdt
# dat<-read.csv("/Volumes/NO NAME/100_1.csv")
dat<-read.csv("/Users/jac/Documents/projects/plantWater/lysim/Lysimeter/logger/data/20170614/100_1.csv",header=FALSE,)
names(dat)<-c("NodeID","unixtime","BatV","ch1Temp","ch1Kg","ch2Temp","ch2Kg","ch3Temp","ch3Kg","ch4Temp","ch4Kg","ch5Temp","ch5Kg","ch6Temp","ch6Kg","ch7Temp","ch7Kg","ch8Temp","ch8Kg")
dat$DateTime<-as.POSIXct(dat$unixtime, origin="1970-01-01",tz="UTC") #rtc is set to UTC
head(dat)
tail(dat)
<<<<<<< HEAD


=======
ggplot(dat[-1,],aes(x=DateTime,y=ch1Kg))+geom_line()
ggplot(dat[-1,],aes(x=DateTime,y=ch1Kg))+geom_line()+geom_line(aes(y=ch2Kg),colour=2)+geom_line(aes(y=ch3Kg),colour=3)+geom_line(aes(y=ch4Kg),colour=4)+geom_line(aes(y=ch5Kg),colour=5)+geom_line(aes(y=ch6Kg),colour=6)+geom_line(aes(y=ch7Kg),colour=7)+geom_line(aes(y=ch8Kg),colour=8)
ggplot(dat[-1,],aes(x=DateTime,y=ch1Temp))+geom_line()+geom_line(aes(y=ch2Temp),colour=2)+geom_line(aes(y=ch3Temp),colour=3)+geom_line(aes(y=ch4Temp),colour=4)+geom_line(aes(y=ch5Temp),colour=5)+geom_line(aes(y=ch6Temp),colour=6)+geom_line(aes(y=ch7Temp),colour=7)+geom_line(aes(y=ch8Temp),colour=8)
ggplot(dat[-1,],aes(x=DateTime,y=ch1Temp))+geom_line()+geom_line(aes(y=ch2Temp),color=2)
testdat<-data.frame(xx=1:8,yy=1:8)
ggplot(testdat,aes(x=xx,y=yy))+geom_point(colour=1:8,size=12)+geom_text(aes(x=xx,y=yy),label=1:8)
>>>>>>> 77d6991d0d2609ecd1e070f8ce7bfc4568973979
