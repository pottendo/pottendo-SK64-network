"<!doctype html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"\t<meta charset=\"utf-8\">\n"
"\t<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, shrink-to-fit=no\">\n"
"\t<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/bootswatch/4.5.2/darkly/bootstrap.min.css\" integrity=\"sha512-8lE+UgnY2CgbE+WDsGwSwAiMOswuRYm11jXYV5KWH6XfSDAzrdRMbPQDpCwzVjJbe9quHrNNPV6N/llRKnw5Hg==\" crossorigin=\"anonymous\" />\n"
"\t\n"
"\t<link rel=\"icon\" href=\"/favicon.ico\" type=\"image/x-icon\">\n"
"\t\n"
"\t<!-- link rel=\"apple-touch-icon\" sizes=\"57x57\" href=\"/apple-icon-57x57.png\">\n"
"\t<link rel=\"apple-touch-icon\" sizes=\"60x60\" href=\"/apple-icon-60x60.png\">\n"
"\t<link rel=\"apple-touch-icon\" sizes=\"72x72\" href=\"/apple-icon-72x72.png\">\n"
"\t<link rel=\"apple-touch-icon\" sizes=\"76x76\" href=\"/apple-icon-76x76.png\">\n"
"\t<link rel=\"apple-touch-icon\" sizes=\"114x114\" href=\"/apple-icon-114x114.png\">\n"
"\t<link rel=\"apple-touch-icon\" sizes=\"120x120\" href=\"/apple-icon-120x120.png\">\n"
"\t<link rel=\"apple-touch-icon\" sizes=\"144x144\" href=\"/apple-icon-144x144.png\">\n"
"\t<link rel=\"apple-touch-icon\" sizes=\"152x152\" href=\"/apple-icon-152x152.png\">\n"
"\t<link rel=\"apple-touch-icon\" sizes=\"180x180\" href=\"/apple-icon-180x180.png\">\n"
"\t<link rel=\"icon\" type=\"image/png\" sizes=\"192x192\"  href=\"/android-icon-192x192.png\">\n"
"\t<link rel=\"icon\" type=\"image/png\" sizes=\"32x32\" href=\"/favicon-32x32.png\">\n"
"\t<link rel=\"icon\" type=\"image/png\" sizes=\"96x96\" href=\"/favicon-96x96.png\">\n"
"\t<link rel=\"icon\" type=\"image/png\" sizes=\"16x16\" href=\"/favicon-16x16.png\">\n"
"\t<link rel=\"manifest\" href=\"/manifest.json\">\n"
"\t<meta name=\"msapplication-TileColor\" content=\"#ffffff\">\n"
"\t<meta name=\"msapplication-TileImage\" content=\"/ms-icon-144x144.png\">\n"
"\t<meta name=\"theme-color\" content=\"#ffffff\" -->\n"
"\t\n"
"\t<title>Sidekick64 Web-Interface</title>\n"
"</head>\n"
"<body>\n"
"\t&nbsp;<br/><div class=\"container\">\n"
"\t\t\n"
"\t<div class=\"container\" style=\"background-color: #4d4d4d; text-align: center; padding-bottom: 8px;\">\n"
"\t\t<img src=\"/sidekick64_logo.png\" style=\"height: 128px; width: 128px;\"/>\n"
"\t</div>\n"
"\t<div class=\"container bg-dark\">\n"
"\t\t<br/>\n"
"\t\t<p>%s</p>\n"
"\n"
"\t\t<form action=\"index.html\" method=\"post\" name=\"upload_form\" enctype=\"multipart/form-data\">\n"
"\t\t\t<div class=\"form-group\">\n"
"\t\t\t\t<div class=\"custom-file\">\n"
"\t\t\t\t  <input type=\"file\" class=\"custom-file-input\" id=\"skUploadFile\" name=\"kernelimg\" >\n"
"\t\t\t\t  <label class=\"custom-file-label\" for=\"skUploadFile\">Choose file</label>\n"
"\t\t\t\t</div>\n"
"\t\t\t\t<br/><br/>\n"
"\t\t\t\t<div class=\"custom-control custom-radio custom-control-inline\">\n"
"\t\t\t\t\t<input checked type=\"radio\" id=\"customRadioInline1\" name=\"radio_saveorlaunch\" value=\"s\" class=\"custom-control-input\">\n"
"\t\t\t\t\t<label class=\"custom-control-label\" for=\"customRadioInline1\">Save file to SD card</label>\n"
"\t\t\t\t</div>\n"
"\t\t\t\t<div class=\"custom-control custom-radio custom-control-inline\">\n"
"\t\t\t\t\t<input type=\"radio\" id=\"customRadioInline2\" name=\"radio_saveorlaunch\" value=\"l\" class=\"custom-control-input\">\n"
"\t\t\t\t\t<label class=\"custom-control-label\" for=\"customRadioInline2\">Launch file</label>\n"
"\t\t\t\t</div>\n"
"\t\t\t\t<div class=\"custom-control custom-radio custom-control-inline\">\n"
"\t\t\t\t\t<input checked type=\"radio\" id=\"customRadioInline3\" name=\"radio_saveorlaunch\" value=\"b\" class=\"custom-control-input\">\n"
"\t\t\t\t\t<label class=\"custom-control-label\" for=\"customRadioInline4\">Save &amp; Launch</label>\n"
"\t\t\t\t</div>\n"
"\t\t\t\t<br/><br/>\n"
"\t\t\t\t<input type=\"submit\" class=\"btn btn-success\" value=\"Upload\" />\n"
"\t\t\t\t<br/><br/>\n"
"\t\t\t</div>\n"
"\t\t</form>\n"
"\n"
"\t\t<form action=\"index.html\" method=\"post\" name=\"config_form\" enctype=\"multipart/form-data\">\n"
"\t\t\t<div class=\"form-group\">\n"
"\t\t\t\t<label for=\"textarea_config\">Edit configuration file SD:C64/sidekick64.cfg</label>\n"
"\t\t    <textarea class=\"form-control\" name=\"textarea_config\" id=\"textarea_config\" rows=\"10\" \n"
"\t\t\t\t\tstyle=\"background-color: #4d4d4d; font-family: monospace; color: #ECFFDC;\">%s</textarea>\n"
"\t\t\t\t<br/>\n"
"\t\t\t\t<div class=\"custom-control custom-radio custom-control-inline\">\n"
"\t\t\t\t\t<input checked type=\"radio\" id=\"radio_configsave1\" name=\"radio_configsavereboot\" value=\"s\" class=\"custom-control-input\">\n"
"\t\t\t\t\t<label class=\"custom-control-label\" for=\"radio_configsave1\">Save file to SD card</label>\n"
"\t\t\t\t</div>\n"
"\t\t\t\t<div class=\"custom-control custom-radio custom-control-inline\">\n"
"\t\t\t\t\t<input type=\"radio\" id=\"radio_configsave2\" name=\"radio_configsavereboot\" value=\"r\" class=\"custom-control-input\">\n"
"\t\t\t\t\t<label class=\"custom-control-label\" for=\"radio_configsave2\">Save file and reboot Sidekick</label>\n"
"\t\t\t\t</div>\n"
"\t\t\t\t<br/><br/>\n"
"\t\t\t\t<input type=\"submit\" class=\"btn btn-success\" value=\"Save\" />\n"
"\t\t\t\t<br/><br/>\n"
"\t\t\t</div>\n"
"\t\t</form>\n"
"\n"
"\t</div>\n"
"\t<p class=\"small\">Find the Sidekick64 project resources <a href=\"https://github.com/frntc/Sidekick64\" target=\"_blank\">at GitHub</a>.</p>\n"
"\t<p class=\"small\">Based on the <a href=\"https://github.com/rsta2/circle\" target=\"_blank\">Circle</a> C++ bare metal environment. Circle %s</a> running on %s</p>\n"
"\t<script src=\"https://code.jquery.com/jquery-3.5.1.slim.min.js\" integrity=\"sha384-DfXdz2htPH0lsSSs5nCTpuj/zy4C+OGpamoFVy38MVBnE+IbbVYUew+OrCXaRkfj\" crossorigin=\"anonymous\"></script>\n"
"\t<script src=\"https://cdn.jsdelivr.net/npm/popper.js@1.16.1/dist/umd/popper.min.js\" integrity=\"sha384-9/reFTGAW83EW2RDu2S0VKaIzap3H66lZH81PoYlFhbGU+6BZp6G7niu735Sk7lN\" crossorigin=\"anonymous\"></script>\n"
"\t<script src=\"https://stackpath.bootstrapcdn.com/bootstrap/4.5.2/js/bootstrap.min.js\" integrity=\"sha384-B4gt1jrGC7Jh4AgTPSdUtOBvfO8shuf57BaghqFfPlYxofvL8/KUEfYiJOMMV+rV\" crossorigin=\"anonymous\"></script>\t\n"
"\t\n"
"\t<script>\n"
"\t$(document).ready(function() {\t\t\n"
"\t\tdocument.querySelector('.custom-file-input').addEventListener('change',function(e){\n"
"\t\t\tvar fileName = document.getElementById(\"skUploadFile\").files[0].name;\n"
"\t\t\tvar nextSibling = e.target.nextElementSibling;\n"
"\t\t\tnextSibling.innerText = fileName;\n"
"\t\t});\n"
"\t});\n"
"\t</script>\n"
"\t\t\n"
"\t</div>\n"
"</body>\n"
"</html>\n"
""
