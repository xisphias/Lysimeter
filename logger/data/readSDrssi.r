library(ggplot2)
library(reshape2)
library(plyr)
options(scipen=9999)
datDir<-"/Users/jac/Documents/projects/plantWater/lysim/Lysimeter/logger/data/20170628/"
datDir<-"C:/Users/Owner/Documents/GitHub/Lysimeter/logger/data/20170809/"
datFiles<-grep("Rssi.csv",list.files(datDir),value=TRUE)
datFiles
setwd(datDir)
outdir<-paste(datDir,"output/",sep='')
dir.create(outdir)
rxDat<-data.frame(NodeID=numeric(0),unixtime=numeric(0),rxrssi=numeric(0),DateTime=character())
  temprxDat<-read.csv(datFiles,header=FALSE,colClasses = c("character",rep("numeric",2)))
  names(temprxDat)<-c("NodeID","unixtime","RxRssi")
  temprxDat$DateTime<-as.POSIXct(temprxDat$unixtime, origin="1970-01-01",tz="America/Anchorage") #rtc is set to UTC
  rxDat<-rbind(rxDat,temprxDat)
tail(rxDat)
unique(rxDat$NodeID)
rxDat$NodeID<-as.factor(rxDat$NodeID)

# ggplot(rxDat,aes(x=DateTime,y=-RxRssi))+geom_point(aes(color=NodeID))+ylim(0,115)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1));ggplot(rxDat[rxDat$DateTime>"2017-07-30",],aes(x=DateTime,y=-RxRssi))+geom_point(aes(color=NodeID))+ylim(0,115)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))ggplot(rxDat,aes(x=DateTime,y=-RxRssi))+geom_line(aes(color=NodeID))+ylim(0,115)+theme(axis.text.x=element_text(angle=30,hjust=1,vjust=1))

