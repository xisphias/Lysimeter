library(ggplot2)
# dat<-read.csv("test_20170601_100_9.csv")
# dat$DateTime<-as.POSIXct(dat$unixtime+(8*60*60), origin="1970-01-01") #corrects for setting rtc to akdt
dat<-read.csv("/Volumes/NO NAME/100_1.csv")
dat<-read.csv("/Volumes/NO NAME/100_2.csv")
dat<-read.csv("/Volumes/NO NAME/100_3.csv")
names(dat)<-c("NodeID","UnixTime","BatV","BoardT","y1g","y1tC","y2g","y2tC","y3g","y3tC","y4g","y4tC","y5g","y5tC","y6g","y6tC","y7g","y7tC","y8g","y8tC","count")
dat$DateTime<-as.POSIXct(dat$UnixTime, origin="1970-01-01",tz="UTC") #rtc is set to UTC
head(dat)
dat$DateTime[1]

ggplot(dat[,],aes(x=DateTime,y=y1g))+geom_line()
ggplot(dat,aes(x=DateTime,y=y1g))+geom_line()+geom_line(aes(y=y2g),colour=2)
ggplot(dat,aes(x=DateTime,y=y1tC))+geom_line()+geom_line(aes(y=y2tC),color=2)
tail(dat)

100 * ((14700+3820) * readings * (3.3/1023)) / 3820
(14700+3820)*1022*3.3/1023
100*(((14700+3820)/1023.0)*(1022*3.3))/3820
