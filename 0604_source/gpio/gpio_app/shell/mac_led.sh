#!/bin/bash

/app/gpio_app 2 3 5 10
while [ 1 ]
do
    /app/gpio_app 1 3 6 100
done
