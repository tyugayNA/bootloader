This logic has beeen implemented using the https://www.teuniz.net/RS-232/
Based on embetronix.com project of bootloder OTA

Run the below command to compile the application.

	gcc main.c RS232\rs232.c -IRS232 -Wall -Wextra -o2 -o app_loader


Once you have build the application, then run the application like below.

		.\app_loader.exe COMPORT_NUM APPLICATION_BIN_PATH
		
		example:
		.\app_loader.exe 8 application.bin