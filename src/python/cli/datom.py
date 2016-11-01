import sys
import os
sys.path.insert(0, os.path.abspath('../../../artifacts/lib/python2.7/dist-packages'))
sys.path.insert(0, os.path.abspath('../gen'))
from thrift import Thrift
from thrift.transport import TSocket
from thrift.transport import TTransport
from thrift.protocol import TBinaryProtocol
from configapi import ConfigApi
from commontypes.ttypes import *
import click
from tabulate import tabulate

def getConfigClient():
    transport = TSocket.TSocket('localhost', 9090)
    transport = TTransport.TBufferedTransport(transport)
    protocol = TBinaryProtocol.TBinaryProtocol(transport)
    client = ConfigApi.Client(protocol)
    transport.open()
    return client
    client.addService()
    transport.close()

@click.group()
def cli():
    pass

@cli.command()
@click.argument('id')
def adddatasphere(id):
    client = getConfigClient()
    info = DataSphereInfo()
    info.id = id
    client.addDatasphere(info)
    click.echo('Successfully added datasphere:{}'.format(id))

@cli.command()
@click.argument('datasphere')
@click.argument('nodeid')
def addvolumeservice(datasphere, nodeid):
    client = getConfigClient()
    info = ServiceInfo()
    info.dataSphereId = datasphere
    info.nodeId = nodeid
    info.id = '{}-vs'.format(nodeid)
    info.type = ServiceType.VOLUME_SERVER
    client.addService(info)
    click.echo('Successfully added service datasphere:{} id:{}'.format(datasphere, info.id))

@cli.command()
@click.argument('datasphere')
@click.argument('name')
def addvolume(datasphere, name):
    client = getConfigClient()
    vol = VolumeInfo()
    vol.datasphereId = datasphere
    vol.name = name
    client.addVolume(vol)
    click.echo('Successfully added volume name:{}'.format(name))

@cli.command()
@click.argument('datasphere')
def listservices(datasphere):
    client = getConfigClient()
    services = client.listServices(datasphere)
    tbl = [[svc.dataSphereId, svc.id, svc.type, svc.ip, svc.port] for svc in services]
    click.echo(tabulate(tbl, headers=['datasphere', 'id', 'type', 'ip', 'port']))

@cli.command()
@click.argument('datasphere')
def listvolumes(datasphere):
    client = getConfigClient()
    volumes = client.listVolumes(datasphere)
    tbl = [[v.id, v.name] for v in volumes]
    click.echo(tabulate(tbl, headers=['id', 'name']))

@cli.command()
@click.argument('datasphere')
def listvolumerings(datasphere):
    client = getConfigClient()
    rings = client.listVolumeRings(datasphere)
    tbl = [[r.id, r.memberIds[0], r.memberIds[1], r.memberIds[2]] for r in rings]
    click.echo(tabulate(tbl, headers=['id', 'member1', 'member2', 'member3']))
    

if __name__ == '__main__':
    cli()
