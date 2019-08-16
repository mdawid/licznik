function onSubmitForm(e){
  var values = e.namedValues;
  var x = values['RSSI'];
  var y = values['Czas'];
  var z = values['Impulsy'].toString();
  Logger.log("On Submit Form: x=" + x + " y=" + y + " z=" + z );
  
  var ss = SpreadsheetApp.getActiveSpreadsheet();
  
  odczyt = ss.getSheetByName('Odczyt');
  var odczytRange = odczyt.getRange(odczyt.getLastRow(), 1 );
  var time = odczytRange.getValue();
  
  var sheet = ss.getSheetByName('T2');
 
  var ticks = [{}];
  ticks = z.split(",");
  var sum = 0;
  for( var i = 0; i < ticks.length; i++ ) {
    sum += Number(ticks[i]);
    //sheet.appendRow([new Date(time.getTime() - ((ticks.length - i ) * y * 1000)), x.toString(), y.toString(), ticks[i]]);
  }
  
  sheet.appendRow([time, x.toString(), sum]);

  checkAlarm();
}

function checkAlarm() {
  var ss = SpreadsheetApp.getActiveSpreadsheet();
  var config = ss.getSheetByName('Woda');
  var sheet = ss.getSheetByName('T2');
  
  // jesli woda leje sie od n-odczytow, to wyslij alarm
  
  var rowCount = config.getRange('K3').getValue();
  var lastRow = sheet.getLastRow();
  var firstRow = Math.max(1, lastRow - rowCount + 1 );
  
  for( var i = firstRow; i <= lastRow; i++ ) {
    var value = sheet.getRange(i, 3 ).getValue();
    Logger.log("Wiersz [" + i + "]=" + value);
    if ( value == 0 )
      return;
  }
  
  // telefon zapisany jest w formacie: operator;numer_MSISDN
  sendAlarm( 'Alarm wodny', "Zakrêæ wodê! Zakrêæ wodê!", config.getRange('K4:K9').getValues() );
}

function sendAlarm( subject, message, phones ) {
    for each (var phone in phones) {
      var phoneInfo = phone.toString().split(';');
      if ( phoneInfo[0] == 'orange' ) {
        MailApp.sendEmail( phoneInfo[1]+"@sms.orange.pl", subject, message );
      } else if ( phoneInfo[0] == 'plus' ) {
        MailApp.sendEmail( phoneInfo[1]+"@text.plusgsm.pl", subject, message );
      }
    }
}