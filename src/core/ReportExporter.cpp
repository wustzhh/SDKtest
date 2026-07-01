#include "ReportExporter.h"
#include "XlsxWriter.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QSet>

// ════════════════════════════════════════════════════════════
//  HTML 导出
// ════════════════════════════════════════════════════════════
bool ReportExporter::exportToHtml(const TestReport& report,
                                   const QString& filePath,
                                   QString* errorMsg)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMsg) *errorMsg = file.errorString();
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    int total = report.total();
    int passed = report.passed();
    int failed = report.failed();
    double passRate = total > 0 ? passed * 100.0 / total : 0;

    QSet<QString> allPropKeys;
    for (const auto& r : report.results)
        for (auto it = r.properties.begin(); it != r.properties.end(); ++it)
            allPropKeys.insert(it.key());
    QStringList propKeys = allPropKeys.values();
    propKeys.sort();

    // HTML 头
    out << "<!DOCTYPE html>\n<html lang=\"zh-CN\">\n<head>\n";
    out << "<meta charset=\"UTF-8\">\n";
    out << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\">\n";
    out << "<title>Test Report</title>\n";
    out << "<style>\n";
    out << "*{margin:0;padding:0;box-sizing:border-box}\n";
    out << "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:linear-gradient(135deg,#e8ecf8,#f0f2f8);color:#1a1a2e;padding:40px 20px;display:flex;justify-content:center}\n";
    out << ".container{max-width:1200px;width:100%}\n";
    out << "h1{font-size:28px;font-weight:300;color:#6c5ce7;margin-bottom:4px;letter-spacing:1px}\n";
    out << "h1 small{font-size:13px;color:#999;margin-left:12px}\n";
    out << ".info{display:flex;gap:20px;flex-wrap:wrap;margin-bottom:28px;padding-bottom:16px;border-bottom:1px solid rgba(108,92,231,0.15);font-size:13px;color:#999}\n";
    out << ".info b{color:#1a1a2e;font-weight:500}\n";
    out << ".stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:14px;margin-bottom:32px}\n";
    out << ".card{background:rgba(255,255,255,0.7);border:1px solid rgba(108,92,231,0.15);border-radius:12px;padding:18px;text-align:center;backdrop-filter:blur(10px);transition:border-color .2s}\n";
    out << ".card:hover{border-color:#6c5ce7}\n";
    out << ".card .n{font-size:30px;font-weight:600}\n";
    out << ".card .l{font-size:11px;color:#999;margin-top:4px;text-transform:uppercase;letter-spacing:1px}\n";
    out << ".card.pass .n{color:#6c5ce7}\n.card.fail .n{color:#ff4757}\n.card.rate .n{color:#ffa502}\n";
    out << "h2{font-size:17px;font-weight:400;color:#6c5ce7;margin:24px 0 14px;letter-spacing:.5px}\n";
    out << ".table-wrap{border-radius:12px}\n";
    out << "table{width:100%;border-collapse:collapse;background:rgba(255,255,255,0.7);border-radius:12px;border:1px solid rgba(108,92,231,0.12)}\n";
    out << "th{background:rgba(108,92,231,0.06);color:#6c5ce7;font-size:10px;font-weight:600;letter-spacing:1px;padding:10px 12px;text-align:left;border-bottom:1px solid rgba(108,92,231,0.1)}\n";
    out << "td{padding:8px 12px;font-size:13px;border-bottom:1px solid rgba(108,92,231,0.04);color:#1a1a2e;word-break:break-all;vertical-align:top}\n";
    out << "tr:hover td{background:rgba(108,92,231,0.04)}\n";
    out << ".ok{color:#6c5ce7;font-weight:500}\n.fail{color:#ff4757;font-weight:500}\n";
    out << ".filter-bar{background:rgba(255,255,255,0.6);border:1px solid rgba(108,92,231,0.12);border-radius:10px;padding:12px 16px;margin-bottom:14px}\n";
    out << ".filter-bar .title{font-weight:600;color:#6c5ce7;font-size:13px;margin-bottom:8px}\n";
    out << ".mode-bar{display:flex;align-items:center;gap:12px;margin-bottom:8px;font-size:12px;padding:4px 0}\n";
    out << ".mode-bar label{cursor:pointer;display:flex;align-items:center;gap:4px}\n";
    out << ".cond-row{display:flex;align-items:center;gap:6px;margin:4px 0;flex-wrap:wrap}\n";
    out << ".cond-row select,.cond-row input{background:#fff;border:1px solid #ddd;border-radius:5px;padding:4px 8px;font-size:12px;color:#333}\n";
    out << ".cond-row select:hover,.cond-row input:hover{border-color:#6c5ce7}\n";
    out << ".cond-row input{min-width:160px}\n";
    out << ".cond-row .rm{color:#999;cursor:pointer;font-size:16px;padding:0 4px}\n";
    out << ".cond-row .rm:hover{color:#ff4757}\n";
    out << ".btn-add{background:#6c5ce7;color:#fff;border:none;border-radius:5px;padding:4px 12px;font-size:12px;cursor:pointer;margin-top:6px}\n";
    out << ".btn-add:hover{background:#5a4bd1}\n";
    out << ".tag{display:inline-block;background:#6c5ce7;color:#fff;font-size:11px;padding:2px 8px;border-radius:4px;margin:2px}\n";
    out << ".tag .x{margin-left:4px;cursor:pointer;opacity:.6}\n";
    out << ".tag .x:hover{opacity:1}\n";
    out << ".filter-count{font-size:12px;color:#999;margin-left:auto}\n";
    out << "</style>\n";
    out << "<datalist id=\"val-suggestions\"></datalist>\n";
    out << "<script>\n";
    out << "var HEADERS=[";
    out << "'" << QString::fromUtf8("\xe7\x8a\xb6\xe6\x80\x81") << "'";
    out << ",'" << QString::fromUtf8("\xe5\xa5\x97\xe4\xbb\xb6") << "'";
    out << ",'" << QString::fromUtf8("\xe7\x94\xa8\xe4\xbe\x8b") << "'";
    for (const auto& k : propKeys) out << ",'" << k.toHtmlEscaped() << "'";
    out << "];\n";
    out << "var CONDITIONS_KEY='\\x00';var OPERATORS=[{v:'in',l:'include'},{v:'eq',l:'='},{v:'ne',l:'!='},{v:'lt',l:'<'},{v:'gt',l:'>'},{v:'le',l:'<='},{v:'ge',l:'>='}];var cs=[];\n";
    out << "function isNumCol(i){var T=document.getElementById('result-table');if(!T)return 0;var R=T.querySelectorAll('tbody tr');for(var z=0;z<R.length;z++){var c=R[z].cells[i];if(c&&c.textContent.trim()&&isNaN(parseFloat(c.textContent)))return 0;}return 1;}\n";
    out << "function G(i){var T=document.getElementById('result-table');if(!T)return[];var R=T.querySelectorAll('tbody tr');var S={};for(var z=0;z<R.length;z++){var c=R[z].cells[i];if(c)S[c.textContent.trim()]=1;}return Object.keys(S).sort();}\n";
    out << "function R(i){var c=cs[i];if(!c)return;var V=G(c.c);var D=document.getElementById('val-suggestions');D.innerHTML='';for(var z=0;z<V.length;z++){var o=document.createElement('option');o.value=V[z];D.appendChild(o);}}\n";
    out << "function A(){var T=document.getElementById('result-table');if(!T||!T.querySelector('tbody tr'))return;cs.push({c:0,o:'in',v:''});P();R(cs.length-1);document.getElementById('mode-bar').style.display=cs.length>=2?'flex':'none';}\n";
    out << "function X(i){cs.splice(i,1);document.getElementById('mode-bar').style.display=cs.length>=2?'flex':'none';P();F();}\n";
    out << "function U(i,f,x){cs[i][f]=x;if(f==='c')R(i);P();if(f!=='v')F();}\n";
    out << "function K(i,e){if(e.key==='Enter'){cs[i].v=e.target.value;F();}}\n";
    out << "function B(i,e){cs[i].v=e.target.value;F();}\n";
    out << "function opLabel(x){for(var z=0;z<OPERATORS.length;z++)if(OPERATORS[z].v===x)return OPERATORS[z].l;return x;}\n";
    out << "function P(){var E=document.getElementById('cond-list');if(!cs.length){E.innerHTML='<div style=\"color:#999;font-size:12px;padding:4px 0\">"+QString::fromUtf8("\xe6\x9a\x82\xe6\x97\xa0\xe7\xad\x9b\xe9\x80\x89\xe6\x9d\xa1\xe4\xbb\xb6\xef\xbc\x8c\xe7\x82\xb9\xe5\x87\xbb\xe4\xb8\x8a\xe6\x96\xb9\xe6\xb7\xbb\xe5\x8a\xa0")+"</div>';return;}var h='';for(var i=0;i<cs.length;i++){var c=cs[i];var nc=isNumCol(c.c);h+='<div class=cond-row><select onchange=\"U('+i+',&apos;c&apos;,parseInt(this.value))\">';for(var z=0;z<HEADERS.length;z++)h+='<option value=\"'+z+'\"'+(c.c===z?' selected':'')+'>'+HEADERS[z]+'</option>';h+='</select><select onchange=\"U('+i+',&apos;o&apos;,this.value)\">';for(var z=0;z<OPERATORS.length;z++){if(!nc&&(OPERATORS[z].v==='lt'||OPERATORS[z].v==='gt'||OPERATORS[z].v==='le'||OPERATORS[z].v==='ge'))continue;h+='<option value=\"'+OPERATORS[z].v+'\"'+(c.o===OPERATORS[z].v?' selected':'')+'>'+OPERATORS[z].l+'</option>';}h+='</select><input list=val-suggestions value=\"'+c.v.replace(/\"/g,'&quot;')+'\" placeholder=\""+QString::fromUtf8("\xe8\xbe\x93\xe5\x85\xa5\xe6\x88\x96\xe9\x80\x89\xe6\x8b\xa9...")+"\" onfocus=\"R('+i+')\" onkeydown=\"K('+i+',event)\" onblur=\"B('+i+',event)\"><span class=rm onclick=\"X('+i+')\">&#x2716;</span></div>';}E.innerHTML=h;}\n";
    out << "function M(v,o,p){var x=(''+v).toLowerCase().trim();var y=(''+p).toLowerCase().trim();if(!y)return 1;switch(o){case'eq':return x===y;case'ne':return x!==y;case'lt':var a=parseFloat(x),b=parseFloat(y);return a<b;case'gt':return a>b;case'le':return a<=b;case'ge':return a>=b;default:return x.indexOf(y)>=0;}}\n";
    out << "function F(){var T=document.getElementById('result-table');if(!T)return;var R=T.querySelectorAll('tbody tr');var n=0;var m=document.querySelector('input[name=m]:checked').value;var ha=0;for(var i=0;i<cs.length;i++)if(cs[i].v)ha=1;for(var z=0;z<R.length;z++){if(!ha){R[z].style.display='';n++;continue;}var ok=(m==='a');for(var i=0;i<cs.length;i++){var c=cs[i];if(!c.v)continue;var cl=R[z].cells[c.c];var mt=cl?M(cl.textContent,c.o,c.v):0;if(i===0){ok=mt;continue;}ok=m==='a'?(ok&&mt):(ok||mt);}R[z].style.display=ok?'':'none';if(ok)n++;}document.getElementById('filter-count').textContent=cs.length?n+'/'+R.length:'';U2();}\n";
    out << "function U2(){var E=document.getElementById('active-tags');var h=[];for(var i=0;i<cs.length;i++){var c=cs[i];if(!c.v)continue;h.push('<span class=tag>'+HEADERS[c.c]+' '+opLabel(c.o)+' '+c.v+' <span class=x onclick=\"X('+i+')\">&#x2716;</span></span>');}E.innerHTML=h.join(' ');}\n";
    out << "window.addEventListener('DOMContentLoaded',function(){P();});\n";
    out << "</script>\n";
    out << "</head>\n<body>\n<div class=\"container\">\n";

    // 标题 + 信息
    out << "<h1>" << QString::fromUtf8("\xe6\xb5\x8b\xe8\xaf\x95\xe6\x8a\xa5\xe5\x91\x8a") << " <small>" << report.testBinary.toHtmlEscaped() << "</small></h1>\n";
    out << "<div class=\"info\">";
    out << "<span>" << QString::fromUtf8("\xe6\x97\xb6\xe9\x97\xb4") << ": <b>" << report.startTime.toString("yyyy-MM-dd hh:mm:ss") << "</b></span>";
    out << "<span>" << QString::fromUtf8("\xe7\xad\x9b\xe9\x80\x89") << ": <b>" << report.filterPattern.toHtmlEscaped() << "</b></span>";
    out << "</div>\n";

    // 统计卡片
    out << "<div class=\"stats\">\n";
    out << "<div class=\"card pass\"><div class=\"n\">" << total << "</div><div class=\"l\">" << QString::fromUtf8("\xe5\x85\xb1\xe8\xae\xa1") << "</div></div>\n";
    out << "<div class=\"card pass\"><div class=\"n\">" << passed << "</div><div class=\"l\">" << QString::fromUtf8("\xe9\x80\x9a\xe8\xbf\x87") << "</div></div>\n";
    out << "<div class=\"card fail\"><div class=\"n\">" << failed << "</div><div class=\"l\">" << QString::fromUtf8("\xe5\xa4\xb1\xe8\xb4\xa5") << "</div></div>\n";
    out << "<div class=\"card rate\"><div class=\"n\">" << QString::number(passRate,'f',1) << "%</div><div class=\"l\">" << QString::fromUtf8("\xe9\x80\x9a\xe8\xbf\x87\xe7\x8e\x87") << "</div></div>\n";
    out << "</div>\n";

    // 筛选器 + 结果表格
    out << "<h2>" << QString::fromUtf8("\xe6\xb5\x8b\xe8\xaf\x95\xe7\xbb\x93\xe6\x9e\x9c") << " <span id=\"filter-count\" style=\"font-size:12px;color:#999;margin-left:10px\"></span></h2>\n";
    out << "<div class=\"filter-bar\">\n";
    out << "  <div class=\"title\">&#x1F50D; " << QString::fromUtf8("\xe7\xad\x9b\xe9\x80\x89\xe6\x9d\xa1\xe4\xbb\xb6") << "</div>\n";
    out << "  <div id=\"mode-bar\" class=\"mode-bar\" style=\"display:none\">\n";
    out << "    <span style=\"color:#6c5ce7;font-weight:600\">" << QString::fromUtf8("\xe6\x9d\xa1\xe4\xbb\xb6\xe5\x85\xb3\xe7\xb3\xbb") << ":</span>\n";
    out << "    <label><input type=\"radio\" name=\"m\" value=\"a\" checked onchange=\"F()\"> AND</label>\n";
    out << "    <label><input type=\"radio\" name=\"m\" value=\"o\" onchange=\"F()\"> OR</label>\n";
    out << "  </div>\n";
    out << "  <div id=\"cond-list\"></div>\n";
    out << "  <button class=\"btn-add\" onclick=\"A()\">+ " << QString::fromUtf8("\xe6\xb7\xbb\xe5\x8a\xa0\xe6\x9d\xa1\xe4\xbb\xb6") << "</button>\n";
    out << "  <div style=\"margin-top:8px;display:flex;gap:10px;align-items:center;flex-wrap:wrap\" id=\"active-tags\"></div>\n";
    out << "</div>\n";
    out << "<div class=\"table-wrap\">\n<table id=\"result-table\">\n<thead><tr><th>" << QString::fromUtf8("\xe7\x8a\xb6\xe6\x80\x81") << "</th><th>" << QString::fromUtf8("\xe5\xa5\x97\xe4\xbb\xb6") << "</th><th>" << QString::fromUtf8("\xe7\x94\xa8\xe4\xbe\x8b") << "</th><th>" << QString::fromUtf8("\xe8\x80\x97\xe6\x97\xb6(ms)") << "</th>";
    for (const auto& k : propKeys) out << "<th>" << k.toHtmlEscaped() << "</th>";
    out << "</tr></thead>\n<tbody>\n";
    for (const auto& r : report.results) {
        out << "<tr><td class=\"" << (r.passed()?"ok":"fail") << "\">" << (r.passed()?QString::fromUtf8("\xe9\x80\x9a\xe8\xbf\x87"):QString::fromUtf8("\xe5\xa4\xb1\xe8\xb4\xa5")) << "</td>";
        out << "<td>" << r.testCase.suiteName.toHtmlEscaped() << "</td>";
        out << "<td>" << r.testCase.caseName.toHtmlEscaped() << "</td>";
        out << "<td>" << QString::number(r.durationMs,'f',1) << "</td>";
        for (const auto& k : propKeys) out << "<td>" << r.properties.value(k).toHtmlEscaped() << "</td>";
        out << "</tr>\n";
    }
    out << "</tbody>\n</table>\n</div>\n";

    // 失败详情
    bool hasFail = false;
    for (const auto& r : report.results) {
        if (r.passed()) continue;
        if (!hasFail) { out << "<h2>" << QString::fromUtf8("\xe5\xa4\xb1\xe8\xb4\xa5\xe8\xaf\xa6\xe6\x83\x85") << "</h2>\n"; hasFail = true; }
        out << "<div style=\"background:rgba(255,71,87,0.06);border:1px solid rgba(255,71,87,0.2);border-radius:10px;padding:12px;margin:8px 0\">\n";
        out << "<strong style=\"color:#ff4757\">" << r.testCase.fullName().toHtmlEscaped() << "</strong>";
        out << " <span style=\"color:#999;font-size:12px\">" << QString::number(r.durationMs,'f',1) << " ms</span>\n";
        if (!r.rawStderr.isEmpty())
            out << "<pre style=\"font-size:12px;color:#e74c3c;margin-top:6px;max-height:200px;overflow:auto;white-space:pre-wrap\">" << r.rawStderr.toHtmlEscaped() << "</pre>\n";
        out << "</div>\n";
    }
    if (!hasFail) out << "<div style=\"color:#2ed573;padding:10px 0\">" << QString::fromUtf8("\xe2\x9c\x93 \xe5\x85\xa8\xe9\x83\xa8\xe5\xb7\xb2\xe9\x80\x9a\xe8\xbf\x87") << "</div>\n";

    out << "</div>\n</body>\n</html>\n";
    file.close();
    return true;
}

// ════════════════════════════════════════════════════════════
bool ReportExporter::exportToXlsx(const TestReport& report,
                                   const QString& filePath,
                                   QString* errorMsg)
{
    XlsxWriter writer;
    QSet<QString> propKeys;
    for (const auto& r : report.results)
        for (auto it = r.properties.begin(); it != r.properties.end(); ++it)
            propKeys.insert(it.key());
    QStringList propKeysSorted = propKeys.values();
    propKeysSorted.sort();
    QStringList headers = { "Suite", "Case", "Status", "Duration(ms)" };
    for (const auto& k : propKeysSorted) headers << k;
    QVector<QStringList> rows;
    for (const auto& r : report.results) {
        QStringList row;
        row << r.testCase.suiteName << r.testCase.caseName
            << r.status << QString::number(r.durationMs, 'f', 1);
        for (const auto& k : propKeysSorted) row << r.properties.value(k, "");
        rows << row;
    }
    writer.addSheet("Test Summary", headers, rows);
    {
        QStringList h = { "Metric", "Value" };
        QVector<QStringList> rows;
        rows << QStringList{ "Total", QString::number(report.total()) };
        rows << QStringList{ "Passed", QString::number(report.passed()) };
        rows << QStringList{ "Failed", QString::number(report.failed()) };
        rows << QStringList{ "Pass Rate", QString::number(report.passed()*100.0/qMax(1,report.total()),'f',1)+"%" };
        rows << QStringList{ "Total Time (ms)", QString::number(report.totalDurationMs(),'f',0) };
        rows << QStringList{ "Time", report.startTime.toString("yyyy-MM-dd hh:mm:ss") };
        rows << QStringList{ "Binary", report.testBinary };
        rows << QStringList{ "Filter", report.filterPattern };
        writer.addSheet("Statistics", h, rows);
    }
    {
        QStringList h = { "Suite", "Case", "Duration(ms)", "Stderr" };
        QVector<QStringList> rows;
        for (const auto& r : report.results) {
            if (r.passed()) continue;
            rows << QStringList{ r.testCase.suiteName, r.testCase.caseName,
                QString::number(r.durationMs,'f',1), r.rawStderr.left(500) };
        }
        if (!rows.isEmpty()) writer.addSheet("Failures", h, rows);
    }
    if (!writer.save(filePath)) {
        if (errorMsg) *errorMsg = writer.lastError();
        return false;
    }
    return true;
}

// ════════════════════════════════════════════════════════════
bool ReportExporter::exportToTxt(const TestReport& report, const QString& filePath, QString* errorMsg)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { if (errorMsg) *errorMsg = file.errorString(); return false; }
    QTextStream out(&file); out.setEncoding(QStringConverter::Utf8);
    QString sep = QString(72, '=');
    out << sep << "\n  Test Report\n  Time: " << report.startTime.toString("yyyy-MM-dd hh:mm:ss")
        << "\n  Binary: " << report.testBinary << "\n  Filter: " << report.filterPattern
        << "\n" << sep << "\n\n--- Statistics ---\n"
        << "  Total: " << report.total() << "\n  Passed: " << report.passed()
        << "\n  Failed: " << report.failed()
        << "\n  Rate: " << QString::number(report.passed()*100.0/qMax(1,report.total()),'f',1) << "%"
        << "\n  Time: " << QString::number(report.totalDurationMs(),'f',0) << " ms\n\n";
    QSet<QString> allPropKeys;
    for (const auto& r : report.results) for (auto it = r.properties.begin(); it != r.properties.end(); ++it) allPropKeys.insert(it.key());
    QStringList propKeys = allPropKeys.values(); propKeys.sort();
    out << "--- All Results ---\n";
    out << QString("%1  %2  %3  %4").arg("Status",-8).arg("Duration",-12).arg("Suite",-30).arg("Case",-30);
    for (const auto& k : propKeys) out << "  " << QString("%1").arg(k,-20);
    out << "\n" << QString(72+propKeys.size()*22,'-') << "\n";
    for (const auto& r : report.results) {
        out << QString("%1  %2  %3  %4").arg(r.passed()?"[PASS]":"[FAIL]",-8)
            .arg(QString::number(r.durationMs,'f',1)+"ms",-12)
            .arg(r.testCase.suiteName,-30).arg(r.testCase.caseName,-30);
        for (const auto& k : propKeys) out << "  " << QString("%1").arg(r.properties.value(k,""),-20);
        out << "\n";
    }
    out << "\n--- " << (report.failed()>0?"Failures":"All Passed") << " ---\n";
    for (const auto& r : report.results) {
        if (r.passed()) continue;
        out << "\n[FAIL] " << r.testCase.fullName() << "  (" << QString::number(r.durationMs,'f',1) << " ms)\n";
        if (!r.rawStderr.isEmpty()) out << "  Stderr:\n" << r.rawStderr.left(1000) << "\n";
    }
    out << "\n" << sep << "\n  End of Report\n" << sep << "\n"; file.close(); return true;
}

// ════════════════════════════════════════════════════════════
bool ReportExporter::exportBoth(const TestReport& report, const QString& basePath, QString* errorMsg)
{
    if (!exportToHtml(report, basePath + ".html", errorMsg)) return false;
    if (!exportToXlsx(report, basePath + ".xlsx", errorMsg)) {
        if (errorMsg) *errorMsg = "HTML ok, XLSX failed: " + *errorMsg;
        return false;
    }
    return true;
}
