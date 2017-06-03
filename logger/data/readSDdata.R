# dat<-read.csv("test_20170601_100_9.csv")
# dat$DateTime<-as.POSIXct(dat$unixtime+(8*60*60), origin="1970-01-01") #corrects for setting rtc to akdt
dat<-read.csv("/Volumes/NO NAME/100_1.csv")
dat$DateTime<-as.POSIXct(dat$unixtime, origin="1970-01-01") #rtc is set to UTC
head(dat)
library(ggplot2)
ggplot(dat[,],aes(x=DateTime,y=ch1KG))+geom_line()
ggplot(dat[dat$ch1KG>3,],aes(x=DateTime,y=ch1KG))+geom_line()+geom_line(aes(y=ch2KG),colour=2)
ggplot(dat,aes(x=DateTime,y=ch1Temp))+geom_line()+geom_line(aes(y=ch2Temp),color=2)
tail(dat)
