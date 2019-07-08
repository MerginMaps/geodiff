import os
import shutil 

nfeatures = 1000000
change_coeff = 100
mod_for_change=1000 * change_coeff
mod_for_add=1600 * change_coeff
mod_for_delete=1800 * change_coeff
epsg=4326
  
vl = QgsVectorLayer('Point?crs=epsg:' + str(epsg), 'layer1', 'memory')
pr = vl.dataProvider()
pr.addAttributes([QgsField("name", QVariant.String),
                  QgsField("rate",  QVariant.Int),
                  QgsField("cost", QVariant.Double)])
vl.updateFields() 

for i in range(1, nfeatures):
    ft1 = QgsFeature()
    ft1.setGeometry(QgsGeometry.fromPointXY(QgsPointXY(i , 0)))
    ft1.setAttributes(["name" + str(i), i, i/100])
    pr.addFeature(ft1)
    
outdir="/Users/peter/Projects/mappin/mergin-sync-prototype/data/simple_many_features"
f1=outdir + "/base" + str(nfeatures)  + "_" + str(change_coeff) + ".gpkg"
f2=outdir + "/modified" + str(nfeatures)  + "_" + str(change_coeff) + ".gpkg"
if os.path.exists(f1):
  os.remove(f1)
QgsVectorFileWriter.writeAsVectorFormat(vl, f1, "utf-8", QgsCoordinateReferenceSystem.fromEpsgId(epsg),"GPKG")
if os.path.exists(f2):
  os.remove(f2)
shutil.copy2(f1, f2)

# NOW EDIT THE OTHER ONE
modified_features = 0

vl = QgsVectorLayer(f2, 'layer2', 'ogr')
pr = vl.dataProvider()
vl.startEditing()
i = 0
for f in vl.getFeatures():
    if i % mod_for_change == 0:
        j = i + 10
        f["name"] = "name" + str(j)
        f["rate"] = j
        f.setGeometry(QgsGeometry.fromPointXY(QgsPointXY(i , 100)))
        modified_features += 1
        
    if i % mod_for_add == 0:
        ft2 = QgsFeature()
        ft2.setGeometry(QgsGeometry.fromPointXY(QgsPointXY(i , 200)))
        ft2.setAttributes(["name_add" + str(i), i, i])
        pr.addFeature(ft2)
        modified_features += 1
        
    if i % mod_for_delete == 0:
        pr.deleteFeatures([f.id()])
        modified_features += 1
        
    i = i + 1

vl.commitChanges()

print(str(modified_features))

# for vl in [vl1, vl2]:
#    vl.updateExtents()
    # QgsProject.instance().addMapLayer(vl)
