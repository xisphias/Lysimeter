library(ggplot2)
library(reshape2)
library(zoo)
library(caTools)
library(plyr)
detach(unload=TRUE,"package:dplyr")
datDir<-"/Users/jac/Documents/projects/plantWater/lysim/Lysimeter/logger/data/20170618/"
datFiles<-grep(".csv",list.files(datDir),value=TRUE)
setwd(datDir)
nNodes<-length(datFiles)
Dat<-data.frame(NodeID=numeric(0),unixtime=numeric(0),BatV=numeric(0),BoardTemp=numeric(0),ch1Kg=numeric(0),ch1Temp=numeric(0),ch2Kg=numeric(0),ch2Temp=numeric(0),ch3Kg=numeric(0),ch3Temp=numeric(0),ch4Kg=numeric(0),ch4Temp=numeric(0),ch5Kg=numeric(0),ch5Temp=numeric(0),ch6Kg=numeric(0),ch6Temp=numeric(0),ch7Kg=numeric(0),ch7Temp=numeric(0),ch8Kg=numeric(0),ch8Temp=numeric(0),count=numeric(0),DateTime=character())
a=1
for(a in 1:nNodes){
  tempDat<-read.csv(datFiles[a],header=FALSE,colClasses = rep("numeric",21))
  names(tempDat)<-c("NodeID","unixtime","BatV","BoardTemp","ch1Kg","ch1Temp","ch2Kg","ch2Temp","ch3Kg","ch3Temp","ch4Kg","ch4Temp","ch5Kg","ch5Temp","ch6Kg","ch6Temp","ch7Kg","ch7Temp","ch8Kg","ch8Temp","count")
  tempDat$DateTime<-as.POSIXct(tempDat$unixtime, origin="1970-01-01",tz="UTC") #rtc is set to UTC
  # head(tempDat)
  # tail(tempDat)
  Dat<-rbind(Dat,tempDat)
}
tail(Dat)
unique(Dat$NodeID)
Dat$ch1Diff<-c(0,diff(Dat$ch1Kg))
Dat$ch2Diff<-c(0,diff(Dat$ch2Kg))
Dat$ch3Diff<-c(0,diff(Dat$ch3Kg))
Dat$ch4Diff<-c(0,diff(Dat$ch4Kg))
Dat$ch5Diff<-c(0,diff(Dat$ch5Kg))
Dat$ch6Diff<-c(0,diff(Dat$ch6Kg))
Dat$ch7Diff<-c(0,diff(Dat$ch7Kg))
Dat$ch8Diff<-c(0,diff(Dat$ch8Kg))
Datm<-melt(data = Dat, id.vars = c("NodeID","unixtime","BatV","BoardTemp","count","DateTime"))
Datm$ch<-factor(substr(Datm$variable,3,3))
Datm$measure<-factor(substr(Datm$variable,4,stop=7))
str(Datm)
Datm2<-melt(Datm,id.vars=c("NodeID","DateTime","BatV","BoardTemp","count",'ch','measure'),measure.vars="value")
str(Datm2)
temp<-Datm2[Datm2$measure=="Temp",]
names(temp)[names(temp)=='value']<-"Temp"
kg<-Datm2[Datm2$measure=="Kg",]
names(kg)[names(kg)=='value']<-"Kg"
diffkg<-Datm2[Datm2$measure=="Diff",]
names(diffkg)[names(diffkg)=='value']<-"diffKg"
Dat2<-merge(temp[,-c(7,8)],kg[,-c(7,8)])
Dat2<-merge(Dat2,diffkg[,-c(7,8)])
Dat2$NodeID<-as.factor(Dat2$NodeID)
Dat2$Kg<-Dat2$Kg/10000
Dat2$diffKg<-Dat2$diffKg/10000
Dat2<-Dat2[Dat2$DateTime>"2016-01-01",]
str(Dat2)
head(Dat2)
Dat2a<-arrange(Dat2,NodeID,ch,DateTime)
head(Dat2a)
Dat3<-ddply(Dat2a,.(NodeID,ch),mutate,rm20min=rollapply(data=Kg,width=4,FUN=mean,fill=0,align="right"),rm1hr=rollapply(data=rm20min,width=3,FUN=mean,fill=0,align="right"),rm1day=rollapply(data=rm1hr,width=24,FUN=mean,fill=NA,align="right"))
Dat3$hrDiff<-c(NA,diff(Dat3$rm1hr))
head(Dat3);tail(Dat3)

ggplot(Dat3,aes(x=DateTime,y=BatV))+geom_line()+ylim(12,13)+facet_wrap(~NodeID)
ggplot(Dat3,aes(x=DateTime,y=BoardTemp))+geom_line()+ylim(-10,25)+facet_wrap(~NodeID)

ggplot(Dat3,aes(x=DateTime,y=diffKg))+geom_line()+ylim(-0.1,0.1)+facet_grid(ch~NodeID)
max(Dat3$DateTime[Dat3$NodeID=='100.9'])
ggplot(Dat3[Dat3$NodeID=='100.1',],aes(x=DateTime,y=diffKg))+geom_line()+ylim(-0.5,0.5)+facet_wrap(~ch)
ggplot(Dat3[Dat2$NodeID=='100.2',],aes(x=DateTime,y=diffKg))+geom_line()+ylim(-0.5,0.5)+facet_wrap(~ch)
ggplot(Dat3[Dat2$NodeID=='100.2',],aes(x=DateTime,y=Kg))+geom_line()+ylim(5,15)+facet_wrap(~ch)
ggplot(Dat2[Dat2$NodeID=='100.5',],aes(x=DateTime,y=Kg))+geom_line()+ylim(5,15)+facet_wrap(~ch)
ggplot(Dat2[Dat2$NodeID=='100.6',],aes(x=DateTime,y=Kg))+geom_line()+ylim(5,15)+facet_wrap(~ch)
ggplot(Dat2[Dat2$NodeID=='100.7',],aes(x=DateTime,y=Kg))+geom_line()+ylim(5,15)+facet_wrap(~ch)
ggplot(Dat2[Dat2$NodeID=='100.8',],aes(x=DateTime,y=Kg))+geom_line()+ylim(5,15)+facet_wrap(~ch)
ggplot(Dat2[Dat2$NodeID=='100.9',],aes(x=DateTime,y=Kg))+geom_line()+ylim(8,13)+facet_wrap(~ch)

ggplot(Dat2[Dat2$NodeID=='100.9'&Dat2$DateTime>'2017-06-14',],aes(x=DateTime,y=Kg))+geom_line()+ylim(5,15)+facet_wrap(~ch)

ggplot(Dat2[Dat2$NodeID=='100.9'&Dat2$DateTime>'2017-06-14',],aes(x=DateTime,y=diffKg))+geom_line()+ylim(-0.2,0.2)+facet_wrap(~ch)


ggplot(Dat3[Dat3$NodeID=='100.2'&Dat3$ch==1,],aes(x=DateTime,y=diffKg*1000))+geom_line()+geom_line(aes(y=hrDiff*1000),color='red')+ylim(-100,100)
ggplot(Dat3[Dat3$NodeID=='100.9'&Dat3$ch==7,],aes(x=DateTime,y=Kg))+geom_line()+geom_line(aes(y=rm20min),color='blue')+geom_line(aes(y=rm1hr),color='red')+geom_line(aes(y=rm1day),color='green')+ylim(12,13)

ggplot(Dat3[Dat3$NodeID=='100.1'&Dat3$ch==7,],aes(x=DateTime,y=Kg))+geom_line()+geom_line(aes(y=rm20min),color='blue')+geom_line(aes(y=rm1hr),color='red')+geom_line(aes(y=rm1day),color='green')+ylim(11,14)
ggplot(Dat3[Dat3$NodeID=='100.7'&Dat3$ch==1,],aes(x=DateTime,y=Kg))+geom_line()+geom_line(aes(y=rm20min),color='blue')+geom_line(aes(y=rm1hr),color='red')+geom_line(aes(y=rm1day),color='green')+ylim(7,9)
ggplot(Dat3[Dat3$NodeID=='100.7',],aes(x=DateTime,y=Kg))+geom_line()+geom_line(aes(y=rm20min),color='blue')+geom_line(aes(y=rm1hr),color='red')+geom_line(aes(y=rm1day),color='green')+ylim(5,15)+facet_wrap(~ch)
ggplot(Dat3[Dat3$NodeID=='100.8',],aes(x=DateTime,y=Kg))+geom_line()+geom_line(aes(y=rm20min),color='blue')+geom_line(aes(y=rm1hr),color='red')+geom_line(aes(y=rm1day),color='green')+ylim(7,13)+facet_wrap(~ch)
ggplot(Dat3[Dat3$NodeID=='100.9'&Dat3$DateTime>'2017-06-15',],aes(x=DateTime,y=Kg))+geom_line()+geom_line(aes(y=rm20min),color='blue')+geom_line(aes(y=rm1hr),color='red')+geom_line(aes(y=rm1day),color='green')+ylim(7,13)+facet_wrap(~ch)
ggplot(Dat3[Dat3$NodeID=='100.2',],aes(x=DateTime,y=Kg))+geom_line()+geom_line(aes(y=rm20min),color='blue')+geom_line(aes(y=rm1hr),color='red')+geom_line(aes(y=rm1day),color='green')+ylim(7,13)+facet_wrap(~ch)
ggplot(Dat3[Dat3$NodeID=='100.1',],aes(x=DateTime,y=Kg))+geom_line()+geom_line(aes(y=rm20min),color='blue')+geom_line(aes(y=rm1hr),color='red')+geom_line(aes(y=rm1day),color='green')+ylim(7,13)+facet_wrap(~ch)

ggplot(dat[-1,],aes(x=DateTime,y=ch1Kg))+geom_line()+geom_line(aes(y=ch2Kg),colour=2)+geom_line(aes(y=ch3Kg),colour=3)+geom_line(aes(y=ch4Kg),colour=4)+geom_line(aes(y=ch5Kg),colour=5)+geom_line(aes(y=ch6Kg),colour=6)+geom_line(aes(y=ch7Kg),colour=7)+geom_line(aes(y=ch8Kg),colour=8)
ggplot(dat[-1,],aes(x=DateTime,y=ch1Temp))+geom_line()+geom_line(aes(y=ch2Temp),colour=2)+geom_line(aes(y=ch3Temp),colour=3)+geom_line(aes(y=ch4Temp),colour=4)+geom_line(aes(y=ch5Temp),colour=5)+geom_line(aes(y=ch6Temp),colour=6)+geom_line(aes(y=ch7Temp),colour=7)+geom_line(aes(y=ch8Temp),colour=8)
ggplot(dat[-1,],aes(x=DateTime,y=ch1Temp))+geom_line()+geom_line(aes(y=ch2Temp),color=2)
testdat<-data.frame(xx=1:8,yy=1:8)
ggplot(testdat,aes(x=xx,y=yy))+geom_point(colour=1:8,size=12)+geom_text(aes(x=xx,y=yy),label=1:8)

