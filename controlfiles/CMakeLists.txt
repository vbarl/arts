
add_custom_command(
  OUTPUT clean-cfiles-cmd
  COMMAND rm -f "Test*.xml" "Test*.rep" "Test*.bin"
  )
add_custom_target (clean-cfiles DEPENDS clean-cfiles-cmd)

add_custom_command(
  OUTPUT mkdir-arts-results-cmd
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/results
  )
add_custom_target (mkdir-arts-results DEPENDS mkdir-arts-results-cmd)



set (ARTS_CONTROLFILE_DIR share/arts/controlfiles)

install (DIRECTORY artscomponents
                   testdata
                   general
                   instruments
         DESTINATION ${ARTS_CONTROLFILE_DIR}
         PATTERN ".svn" EXCLUDE)


#######################
### ARTS Components ###
arts_test_run_ctlfile(fast artscomponents/helpers/TestForloop.arts)
arts_test_run_ctlfile(fast artscomponents/helpers/TestAgendaCopy.arts)
arts_test_run_ctlfile(fast artscomponents/helpers/TestHSE.arts)

arts_test_run_ctlfile(fast artscomponents/agendas/TestAgendaExecute.arts)
arts_test_run_ctlfile(fast artscomponents/agendas/TestArrayOfAgenda.arts)

arts_test_run_ctlfile(fast artscomponents/absorption/TestAbs.arts)
arts_test_run_ctlfile(fast
                      artscomponents/absorption/TestAbsDoppler.arts)
arts_test_run_ctlfile(slow
                      artscomponents/absorption/TestAbsParticle.arts)
arts_test_run_ctlfile(slow artscomponents/absorption/TestIsoRatios.arts)

arts_test_run_ctlfile(fast artscomponents/ppath/TestPpath1D.arts)
arts_test_run_ctlfile(fast artscomponents/ppath/TestPpath2D.arts)
arts_test_run_ctlfile(fast artscomponents/ppath/TestPpath3D.arts)

arts_test_run_ctlfile(fast artscomponents/pencilbeam/TestPencilBeam.arts)

arts_test_run_ctlfile(fast artscomponents/clearsky/TestClearSky.arts)
arts_test_run_ctlfile(slow artscomponents/clearsky/TestClearSky2.arts)
arts_test_run_ctlfile(slow artscomponents/clearsky/TestBatch.arts)

arts_test_run_ctlfile(fast artscomponents/stokesrot/TestStokesRotation.arts)
arts_test_run_ctlfile(fast artscomponents/stokesrot/TestSensorPol.arts)

arts_test_run_ctlfile(fast artscomponents/regrid/TestRegrid.arts)
arts_test_run_ctlfile(fast artscomponents/regrid/TestRegridFields.arts)

arts_test_run_ctlfile(fast artscomponents/groundbased/TestFgrid.arts)
arts_test_run_ctlfile(fast artscomponents/groundbased/TestGbased.arts)
arts_test_run_ctlfile(fast artscomponents/groundbased/TestWinds.arts)

arts_test_run_ctlfile(fast artscomponents/transmission/TestTransmission.arts)
arts_test_run_ctlfile(fast artscomponents/transmission/TestTransmissionWithScat.arts)

arts_test_run_ctlfile(fast artscomponents/tessem/TestTessem.arts)

arts_test_run_ctlfile(fast artscomponents/faraday/TestFaradayRotation.arts)

arts_test_run_ctlfile(fast artscomponents/nlte/TestNLTE.arts)
arts_test_run_ctlfile(fast artscomponents/nlte/TestPopulationDensityNLTE_lineshape.arts)
arts_test_run_ctlfile(slow artscomponents/nlte/testRotationalConvergence.arts)

arts_test_run_ctlfile(fast artscomponents/lineshapes/TestDoppler.arts)
arts_test_run_ctlfile(fast artscomponents/lineshapes/TestLorentz.arts)
arts_test_run_ctlfile(fast artscomponents/lineshapes/TestLorentzLM.arts)
arts_test_run_ctlfile(fast artscomponents/lineshapes/TestVoigt.arts)
arts_test_run_ctlfile(fast artscomponents/lineshapes/TestVoigtLM.arts)
arts_test_run_ctlfile(fast artscomponents/lineshapes/TestHTP-VP.arts)
arts_test_run_ctlfile(fast artscomponents/lineshapes/TestSDVP.arts)
arts_test_run_ctlfile(fast artscomponents/lineshapes/TestHTP.arts)
arts_test_run_ctlfile(fast artscomponents/lineshapes/TestHTPLM.arts)

#arts_test_run_ctlfile(slow artscomponents/radiolink/TestRadioLink.arts)
#arts_test_run_ctlfile(fast artscomponents/radiolink/TestRadioLink2.arts)
#arts_test_run_ctlfile(slow artscomponents/radiolink/TestRadioOccultation.arts)

arts_test_run_ctlfile(fast artscomponents/doit/TestDOIT.arts)
arts_test_run_ctlfile(slow artscomponents/doit/TestDOITaccelerated.arts)
arts_test_run_ctlfile(fast artscomponents/doit/TestDOITprecalcInit.arts)
arts_test_ctlfile_depends(fast.artscomponents.doit.TestDOITprecalcInit
                          fast.artscomponents.doit.TestDOIT)
arts_test_run_ctlfile(fast artscomponents/doit/TestDOITsensorInsideCloudbox.arts)
arts_test_run_ctlfile(slow artscomponents/doitbatch/TestDOITBatch.arts)
arts_test_run_ctlfile(slow artscomponents/doit/TestDOITFromIndividualFields.arts)

arts_test_run_ctlfile(fast artscomponents/montecarlo/TestMonteCarloDataPrepare.arts)
arts_test_run_ctlfile(slow artscomponents/montecarlo/TestMonteCarloGeneral.arts)
arts_test_ctlfile_depends(slow.artscomponents.montecarlo.TestMonteCarloGeneral
                          fast.artscomponents.montecarlo.TestMonteCarloDataPrepare)
arts_test_run_ctlfile(slow artscomponents/montecarlo/TestMonteCarloGeneralGaussian.arts)
arts_test_ctlfile_depends(slow.artscomponents.montecarlo.TestMonteCarloGeneralGaussian
                          fast.artscomponents.montecarlo.TestMonteCarloDataPrepare)
arts_test_run_ctlfile(fast artscomponents/montecarlo/TestRteCalcMC.arts)
arts_test_ctlfile_depends(fast.artscomponents.montecarlo.TestRteCalcMC
                          fast.artscomponents.montecarlo.TestMonteCarloDataPrepare)

arts_test_run_ctlfile(fast artscomponents/wfuns/TestTjacStokes1.arts)
arts_test_run_ctlfile(slow artscomponents/wfuns/TestTjacStokes4_transmission.arts)
arts_test_run_ctlfile(slow artscomponents/wfuns/TestWfuns.arts)
arts_test_run_ctlfile(slow artscomponents/wfuns/TestWfunsHybClear.arts)
arts_test_ctlfile_depends(slow.artscomponents.wfuns.TestWfunsHybClear
                          slow.artscomponents.wfuns.TestWfuns)

arts_test_run_ctlfile(fast artscomponents/radar/TestIyActive.arts)

arts_test_run_ctlfile(fast artscomponents/ycalcappend/TestYCalcAppend.arts)

arts_test_run_ctlfile(fast artscomponents/heatingrates/TestHeatingRates.arts)

arts_test_run_ctlfile(fast artscomponents/dobatch/TestDOBatch.arts)

if (ENABLE_FFTW)
  arts_test_run_ctlfile(fast artscomponents/hitran-xsec/TestHitranXsec.arts)
endif ()

if (ENABLE_DISORT)
  arts_test_run_ctlfile(fast artscomponents/disort/TestDISORT.arts)
endif ()

if (ENABLE_RT4)
  arts_test_run_ctlfile(fast artscomponents/polradtran/TestRT4.arts)
  arts_test_run_ctlfile(fast artscomponents/hybridscat/TestHybrid.arts)
  arts_test_run_ctlfile(slow artscomponents/wfuns/TestWfunsHybCloudy.arts)
endif ()

if (ENABLE_TMATRIX)
  arts_test_run_ctlfile(fast artscomponents/tmatrix/TestTMatrix.arts)
endif ()

if (ENABLE_FASTEM)
  arts_test_run_ctlfile(fast artscomponents/fastem/TestFastem.arts)
  arts_test_run_ctlfile(fast artscomponents/antenna/TestAntenna2D.arts)
  arts_test_run_ctlfile(fast artscomponents/wfuns/TestTjacStokes1to4.arts)
  arts_test_run_ctlfile(slow artscomponents/wfuns/TestTjacStokes4.arts)
endif ()

if (ENABLE_REFICE)
  arts_test_run_ctlfile(fast artscomponents/refice/TestRefice.arts)
endif ()

if (ENABLE_NETCDF)
  arts_test_run_ctlfile(slow artscomponents/moltau/TestMolTau.arts)
endif ()

# if (ENABLE_RELMAT)
#   if (FASTWIGNER)
#     arts_test_run_ctlfile(fast artscomponents/linemixing/TestRelmat.arts)
#   else()
#     arts_test_run_ctlfile(slow artscomponents/linemixing/TestRelmat.arts)
#   endif()
# endif ()

if (OEM_SUPPORT)
  arts_test_run_ctlfile(fast artscomponents/oem/TestOEM.arts)
endif ()

###################
### Instruments ###
arts_test_run_ctlfile(fast instruments/odinsmr/TestOdinSMR.arts)
arts_test_run_ctlfile(slow instruments/odinsmr/TestOdinSMR_1D.arts)

arts_test_run_ctlfile(fast instruments/amsu/TestAMSUB_fast.arts)
arts_test_run_ctlfile(slow instruments/amsu/TestAMSUB.arts)
arts_test_run_ctlfile(slow instruments/amsu/TestAMSUB_MetMM.arts)

arts_test_run_ctlfile(nocheck instruments/amsu/TestAMSUA.arts)

arts_test_run_ctlfile(fast instruments/ici/TestICI_fast.arts)

arts_test_run_ctlfile(fast instruments/mhs/TestMHS_fast.arts)
arts_test_run_ctlfile(slow instruments/mhs/TestMHS.arts)

arts_test_run_ctlfile(slow instruments/hirs/TestHIRS_fast.arts)
arts_test_run_ctlfile(nocheck instruments/hirs/TestHIRS_reference.arts)

arts_test_run_ctlfile(slow instruments/metmm/TestMetMM.arts)

#arts_test_run_ctlfile(slow instruments/avhrr/TestAVHRR.arts)

arts_test_run_ctlfile(nocheck instruments/seviri/TestSEVIRI.arts)
arts_test_run_ctlfile(nocheck instruments/mviri/TestMVIRI.arts)

if (ARTS_XML_DATA_DIR)

  ###############################
  ### testing XML Data itself ###
  arts_test_run_ctlfile(xmldata artscomponents/arts-xml-data/TestAtmScen_Mars.arts)
  arts_test_run_ctlfile(xmldata artscomponents/arts-xml-data/TestAtmScen_Venus.arts)
  arts_test_run_ctlfile(xmldata artscomponents/arts-xml-data/TestAtmScen_Jupiter.arts)

  arts_test_run_ctlfile(xmldata artscomponents/arts-xml-data/TestAtmScenSup_Earth.arts)
  arts_test_run_ctlfile(xmldata artscomponents/arts-xml-data/TestAtmScenSup_Mars.arts)
  arts_test_run_ctlfile(xmldata artscomponents/arts-xml-data/TestAtmScenSup_Venus.arts)
  arts_test_run_ctlfile(xmldata artscomponents/arts-xml-data/TestAtmScenSup_Jupiter.arts)

  arts_test_run_ctlfile(xmldata artscomponents/clearsky/TestClearSkyTips.arts)

#  arts_test_run_ctlfile(xmldata artscomponents/arts-xml-data/TestClouds_Mars.arts)
#  arts_test_run_ctlfile(xmldata artscomponents/arts-xml-data/TestClouds_Venus.arts)

  arts_test_run_ctlfile(xmldata artscomponents/arts-xml-data/TestSurf_Earth.arts)
  arts_test_run_ctlfile(xmldata artscomponents/arts-xml-data/TestSurf_Mars.arts)
  arts_test_run_ctlfile(xmldata artscomponents/arts-xml-data/TestSurf_Venus.arts)

  arts_test_run_ctlfile(xmldata artscomponents/arts-xml-data/TestPlanetIsoRatios.arts)

  arts_test_run_ctlfile(xmldata artscomponents/cia/TestCIA.arts)

  arts_test_run_ctlfile(xmldata artscomponents/cia/TestRTwithCIA.arts)

  arts_test_run_ctlfile(xmldata artscomponents/doit/TestDOITpressureoptimization.arts)

  ###########################################
  ### cases USING data from arts-xml-data ###
  arts_test_run_ctlfile(fast artscomponents/helpers/TestRegridding.arts)
  arts_test_run_ctlfile(fast artscomponents/helpers/TestGridsExtract.arts)

  arts_test_run_ctlfile(fast artscomponents/ppath/TestRefractPlanets.arts)

  arts_test_run_ctlfile(fast artscomponents/zeeman/TestZeeman.arts)
  
  arts_test_run_ctlfile(fast artscomponents/wfuns/TestSpectroscopy.arts)
  
  arts_test_run_ctlfile(slow artscomponents/wfuns/TestDoitJacobians.arts)

  ##################################
  ### planet toolbox demos cases ###
#  arts_test_run_ctlfile(planettoolbox planetary_toolbox/demos/active/DemoLinkBudget.arts)

  #arts_test_run_ctlfile(planettoolbox planetary_toolbox/demos/passive/DemoJupiter_fullRT_1D_clearsky.arts)
  #arts_test_run_ctlfile(planettoolbox planetary_toolbox/demos/passive/DemoMars_fullRT_1D_clearsky.arts)
  #arts_test_run_ctlfile(planettoolbox planetary_toolbox/demos/passive/DemoMars_fullRT_3D_clearsky.arts)
#  arts_test_run_ctlfile(planettoolbox planetary_toolbox/demos/passive/DemoMars_fullRT_1D_cloudy.arts)
  arts_test_run_ctlfile(planettoolbox planetary_toolbox/demos/passive/DemoVenus_fullRT_1D_clearsky.arts)
  #arts_test_run_ctlfile(planettoolbox planetary_toolbox/demos/passive/DemoVenus_fullRT_1D_cloudy.arts)

  arts_test_run_ctlfile(fast artscomponents/cia/TestCIADerivs.arts)
endif (ARTS_XML_DATA_DIR)



