from django.shortcuts import render

# Create your views here.
import math
import statistics

from django.http import HttpResponseRedirect, HttpResponse, Http404
from django.core.urlresolvers import reverse

# django uses protection from Cross Site Request Forgery (CSRF)
# therefore you need to make an exception for logdata POST request 
from django.views.decorators.csrf import csrf_exempt

from django.contrib.auth.models import User
from django.utils import timezone
import datetime, pytz, sys, logging

from .models import Horse, HorseTracker, HorseData
from django.views import generic
from django.template import RequestContext, loader
from django.conf import settings
from django.shortcuts import redirect
from django.contrib.auth.decorators import login_required
from django.contrib.auth.decorators import permission_required
from django.shortcuts import get_object_or_404, render_to_response
# ...

@login_required
def horsetracker_list(request):
    current_user = request.user
    horsetrackers = HorseTracker.objects.filter(user=current_user)
    return render(request, 'horsetracker_list.html', {'horsetrackers': horsetrackers})


@login_required
def horsetracker_detail(request, trackerID):
    pk = trackerID.split(":")[0]
    current_user = request.user
    horsetracker = get_object_or_404(HorseTracker, pk=pk)
    horsedata = HorseData.objects.filter(tracker=horsetracker).order_by('-date')
    if( horsetracker.user != current_user ):
        raise Http404
    return render(request, 'horsetracker_detail.html', {'horsetracker': horsetracker, 'horsedatas': horsedata})


@login_required
def horse_list(request):
    current_user = request.user
    horses = Horse.objects.filter(user=current_user)
    return render(request, 'horse_list.html', {'horses': horses})


@login_required
def horse_detail(request, slug, id):
    current_user = request.user
    horse = get_object_or_404(Horse, pk=id)
    if( horse.user != current_user ):
        raise Http404
    return render(request, 'horse_detail.html', {'horse': horse})


@csrf_exempt
def tracker_add_data(request):

    t_version = request.POST['ver']      # version of software/hardware
    t_imei    = request.POST['IMEI']     # imei (modem) number
    t_imsi    = request.POST['IMSI']     # imsi (simcard) number
    #t_email   = request.POST['user']     # users email adress
    t_email    = request.POST['user']     # users username
    data      = request.POST['data']     # logger data
    sum       = int(request.POST['sum']) # bit sum of sent string (checksum)
    
    # Assembley string for bit sum check
    check_string =("ver=" + t_version + "&IMEI=" + t_imei + "&IMSI=" + 
                t_imsi + "&user=" + t_email + "&data=" + data + "&sum=")
    check_sum = ''.join(format(ord(x), 'b') for x in check_string).count('1')

    # split up data string into each log event
    data_array = data.split('#')
    length = len(data_array) 

    if check_sum == sum:
       
        for i in range(0,length,17):
            #if version == 1:

                value0 = int(data_array[i])     #batt %
                value1 = int(data_array[i+1])   #batt milliVolt
                value2 = float(data_array[i+2]) #BME280 temp / DHT11 temp
                value3 = float(data_array[i+3]) #BME280 hum / DHT11 hum
                value4 = float(data_array[i+4]) #BME280 pressure
                value5 = float(data_array[i+5]) #IR temp
                value6 = float(data_array[i+6]) #MPU temp
                value7 = int(data_array[i+7])   #acx
                value8 = int(data_array[i+8])   #acy
                value9 = int(data_array[i+9])   #acz
                value10 = int(data_array[i+10]) #max
                value11 = int(data_array[i+11]) #may
                value12 = int(data_array[i+12]) #maz

                lat = float(data_array[i+13])
                lon = float(data_array[i+14])

                hour = int(data_array[i+15][0] + data_array[i+15][1])
                min = int(data_array[i+15][2] + data_array[i+15][3])
                sec = int(data_array[i+15][4] + data_array[i+15][5])
 
                day = int(data_array[i+16][0] + data_array[i+16][1])
                month = int(data_array[i+16][2] + data_array[i+16][3])
                year = int('20' + data_array[i+16][4] + data_array[i+16][5])


                logdate = datetime.datetime(year, month, day, hour, min, 
                                        sec,tzinfo=pytz.timezone('UTC'))
                #how to print to mundilfare@uwsgi logg
                #print(datum, file=sys.stderr)

                # grab the user from the database reported by the tracker
                try: 
                    current_user = User.objects.get(username=t_email)
                    #current_user = User.objects.get(email=t_email)
                except User.DoesNotExist:
                    print("DATA:User Does Not Exist", file=sys.stderr)
                    return HttpResponse("ERROR")

                # with user object grab the corresponding tracker object
                # if not existing, then register a new tracker
                try:
                    current_tracker = HorseTracker.objects.get(IMEI=t_imei, IMSI=t_imsi,
                                                       user=current_user)
                    if( current_tracker.version != t_version ):
                        current_tracker.version = t_version
                        current_tracker.save()

                except HorseTracker.DoesNotExist:
                    print("DATA:New tracker found, registering it!", file=sys.stderr)
                    current_tracker = HorseTracker(IMEI=t_imei, IMSI=t_imsi, 
                                               user=current_user, version=t_version)
                    current_tracker.save()
                    print(current_tracker, file=sys.stderr)

                # create a new post of tracker positioning data
                incoming_data = HorseData(tracker=current_tracker, date=logdate,
                         latitude=lat, longitude=lon,
                         pressure=value4, humidity=value3, temperature=value2,
                         IRtemp=value5, MPUtemp=value6, 
                         accX=value7, accY=value8, accZ=value9,
                         magX=value10, magY=value11, magZ=value12,
                         batteryVoltage=value1, batteryCharge=value0
                )
                incoming_data.save()
        return HttpResponse("OK")
    return HttpResponse("ERROR")

@login_required
def googlemap_intensity(request, trackerID):
    pk = trackerID.split(":")[0]
    current_user = request.user
    horsetracker = get_object_or_404(HorseTracker, pk=pk)
    horsedata = HorseData.objects.filter(tracker=horsetracker).order_by('-date')
    if( horsetracker.user != current_user ):
        raise Http404

    #cur = g.db.execute('select latitude, longitude, value4 from positions ' )
    #posData=[] 
    #for row in cur.fetchall():
    #  posData.append(row)
    
    #dataTransp = list(map(list, zip(*posData)))
    
    #convert strings to float, int
    latitude = horsedata.values('latitude')
    longitude = horsedata.values('longitude')
    temperature = horsedata.values('temperature')

    #latitude width
    latitude_list = [x['latitude'] for x in latitude]
    longitude_list = [x['longitude'] for x in longitude]
    temperature_list = [x['temperature'] for x in temperature]

    width = int( (max(latitude_list)-min(latitude_list) )*10000)
      
    #longitude width
    height = int( (max(longitude_list)-min(longitude_list) )*10000)
    
    #ortsjon
    latCenter = statistics.median(latitude_list)#6216576
    lonCenter = statistics.median(longitude_list)#1717866

    density = 3
   
    points=[ dict(latitude=str(latCenter),longitude=str(lonCenter),value2=str(0) )]
    latScaleFactor=math.ceil(1.0/math.sin(math.radians( max(latitude_list)) ))
    for y in range(int(min(latitude_list)*100000)-1, int(max(latitude_list)*100000)+1, density):
      for x in range(int(min(longitude_list)*100000)-1, int(max(longitude_list)*100000)+1, density*latScaleFactor):    
    #for y in range(latCenter-50,latCenter+50,5):
    #  for x in range(lonCenter-150,lonCenter+50,5*latScaleFactor):
       
        avg_count=1
        temp=0
        for i in range(len(temperature_list)):
         
          x0 = int(float(longitude_list[i])*100000)
          y0 = int(float(latitude_list[i])*100000)
           
          if( ( abs(x-x0) + abs(y-y0)*latScaleFactor ) < 3*latScaleFactor ):
            temp = temp + (temperature_list[i])
            avg_count = avg_count + 1
        temp = temp / avg_count
        #if(temp != 0):
        points.append( dict(latitude=str(y/100000.0),longitude=str(x/100000.0),value2=str(temp) ) )


    return render(request, 'googlemap_intensity.html', {'horsetracker': horsetracker, 'horsedatas': horsedata, 'points': points})



