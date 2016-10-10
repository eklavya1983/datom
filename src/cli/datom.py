import click

@click.command()
def hello():
    click.echo("Hello")

if __name__ == '__main__':
    hello()
